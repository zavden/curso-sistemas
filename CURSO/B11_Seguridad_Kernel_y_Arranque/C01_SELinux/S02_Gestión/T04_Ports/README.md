# Ports: semanage port y servicios en puertos no estándar

## Índice

1. [Por qué SELinux controla puertos](#1-por-qué-selinux-controla-puertos)
2. [Tipos de puerto](#2-tipos-de-puerto)
3. [Consultar puertos: semanage port -l](#3-consultar-puertos-semanage-port--l)
4. [Operaciones name_bind y name_connect](#4-operaciones-name_bind-y-name_connect)
5. [Agregar puertos: semanage port -a](#5-agregar-puertos-semanage-port--a)
6. [Modificar y eliminar puertos](#6-modificar-y-eliminar-puertos)
7. [Escenarios prácticos](#7-escenarios-prácticos)
8. [Integración con el flujo de troubleshooting](#8-integración-con-el-flujo-de-troubleshooting)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Por qué SELinux controla puertos

Hasta ahora hemos visto que SELinux etiqueta **archivos** (file contexts) y
**procesos** (domains). Pero hay una tercera dimensión crítica: los **puertos
de red**.

Sin control de puertos, un servicio comprometido podría:

```
Escenario sin control de puertos
================================

  httpd_t comprometido
       │
       ├── Escuchar en puerto 4444 (backdoor)
       ├── Conectar a puerto 25 (enviar spam)
       └── Conectar a puerto 22 (pivotear SSH)

  DAC no puede impedirlo: root puede bind() a cualquier puerto
```

SELinux resuelve esto asignando un **tipo** a cada puerto, exactamente como
hace con archivos:

```
Analogía: tres dimensiones de etiquetado
=========================================

  Archivos:    /var/www/html/index.html  →  httpd_sys_content_t
  Procesos:    /usr/sbin/httpd           →  httpd_t
  Puertos:     TCP 80                    →  http_port_t

  La política dice:
    allow httpd_t http_port_t:tcp_socket name_bind;
    ───── ─────── ────────────           ─────────
    domain  port type                    operación

  Si httpd_t intenta bind() en un puerto que NO es http_port_t → AVC denied
```

El resultado es un **confinamiento tridimensional**:

```
                    ┌─────────────────────┐
                    │   Política SELinux   │
                    └──────────┬──────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
        ┌─────┴─────┐   ┌─────┴─────┐   ┌─────┴─────┐
        │  Archivos  │   │ Procesos  │   │  Puertos  │
        │ (types)    │   │ (domains) │   │  (types)  │
        └───────────┘   └───────────┘   └───────────┘

  httpd_t puede:
    ✓ Leer httpd_sys_content_t
    ✓ Bind en http_port_t
    ✗ Leer shadow_t
    ✗ Bind en ssh_port_t
```

---

## 2. Tipos de puerto

Cada puerto (o rango de puertos) tiene asignado un tipo SELinux. Los tipos
siguen la convención `servicio_port_t`:

### Tipos más comunes

| Tipo SELinux | Puertos por defecto | Servicio |
|---|---|---|
| `http_port_t` | 80, 443, 488, 8008, 8009, 8443 | Apache, Nginx |
| `ssh_port_t` | 22 | OpenSSH |
| `dns_port_t` | 53 | BIND, named |
| `smtp_port_t` | 25, 465, 587 | Postfix, Sendmail |
| `pop_port_t` | 110, 995 | Dovecot POP3 |
| `imap_port_t` | 143, 993 | Dovecot IMAP |
| `mysqld_port_t` | 3306 | MySQL/MariaDB |
| `postgresql_port_t` | 5432 | PostgreSQL |
| `mongod_port_t` | 27017-27019 | MongoDB |
| `redis_port_t` | 6379 | Redis |
| `squid_port_t` | 3128, 3401, 4827 | Squid |
| `nfs_port_t` | 2049 | NFS |
| `samba_port_t` | 137-139, 445 | Samba |
| `ftp_port_t` | 21, 989, 990 | vsftpd |
| `kerberos_port_t` | 88, 749 | Kerberos |
| `ldap_port_t` | 389, 636 | OpenLDAP, 389DS |
| `zabbix_port_t` | 10050, 10051 | Zabbix |
| `unreserved_port_t` | 61000-65535 | Puertos no asignados |
| `hi_reserved_port_t` | 512-1023 | Puertos reservados altos |

### Tipo especial: unreserved_port_t

Los puertos altos (por encima de 1024) que no tienen asignación específica
caen en categorías genéricas:

```
Clasificación de puertos por tipo SELinux
==========================================

  0-511        → Asignados individualmente o reserved_port_t
  512-1023     → hi_reserved_port_t (si no tienen asignación específica)
  1024-32767   → Pueden tener asignación específica o reserved_port_t
  32768-60999  → ephemeral / reserved
  61000-65535  → unreserved_port_t
```

> **Clave**: un puerto puede tener **múltiples tipos** si diferentes políticas
> lo incluyen. Y un tipo puede abarcar **múltiples puertos**. La relación es
> muchos-a-muchos.

---

## 3. Consultar puertos: semanage port -l

### Listar todos los puertos

```bash
# Listar TODOS los puertos etiquetados
semanage port -l

# Salida (extracto):
# SELinux Port Type     Proto    Port Number
# http_port_t           tcp      80, 443, 488, 8008, 8009, 8443
# ssh_port_t            tcp      22
# smtp_port_t           tcp      25, 465, 587
# mysqld_port_t         tcp      3306
```

La salida tiene tres columnas:

```
semanage port -l
─────────────────
SELinux Port Type     Proto    Port Number
─────────────────     ─────    ───────────
http_port_t           tcp      80, 443, 488, 8008, 8009, 8443
│                     │        │
│                     │        └── Puerto(s) asignados
│                     └── Protocolo (tcp o udp)
└── Tipo SELinux del puerto
```

### Filtrar por servicio o puerto

```bash
# Buscar puertos de un servicio específico
semanage port -l | grep http
# http_cache_port_t            tcp      8080, 8118, ...
# http_port_t                  tcp      80, 443, 488, 8008, 8009, 8443

# Buscar qué tipo tiene un puerto específico
semanage port -l | grep -w 8080
# http_cache_port_t            tcp      8080, 8118, ...

# Buscar todos los puertos TCP de un tipo
semanage port -l | grep ssh
# ssh_port_t                   tcp      22
```

### Ver solo modificaciones locales

```bash
# Solo puertos añadidos/modificados por el administrador
semanage port -l -C

# Inicialmente vacío — solo muestra cambios respecto a la política base
```

Este flag `-C` (de "Customized") es muy útil para auditorías: muestra
exactamente qué puertos ha añadido el administrador versus lo que trae la
política por defecto.

### Verificar un puerto específico con sepolicy

```bash
# ¿Qué puertos puede usar httpd_t para bind?
sepolicy network -t http_port_t

# ¿Qué dominios pueden bind en un tipo de puerto?
sepolicy network -t http_port_t -p tcp
```

> **Nota**: `sepolicy` viene del paquete `policycoreutils-devel` (RHEL) o
> `sepolicy` (Fedora). No siempre está instalado por defecto.

---

## 4. Operaciones name_bind y name_connect

SELinux distingue dos operaciones de red fundamentales:

### name_bind: escuchar en un puerto

Cuando un servicio hace `bind()` + `listen()` para aceptar conexiones:

```
name_bind — el servicio ESCUCHA
================================

  httpd_t  ──bind()──►  TCP 80 (http_port_t)     ✓ Permitido
  httpd_t  ──bind()──►  TCP 9090 (???)           ✗ AVC denied

  Regla necesaria:
    allow httpd_t http_port_t:tcp_socket name_bind;
```

El AVC de un `name_bind` denegado:

```
type=AVC msg=audit(...): avc:  denied  { name_bind }
  for  pid=1234 comm="httpd"
  src=9090                          ← puerto que intentó usar
  scontext=system_u:system_r:httpd_t:s0
  tcontext=system_u:object_r:reserved_port_t:s0
                              ↑
                     tipo actual del puerto 9090
  tclass=tcp_socket permissive=0
```

### name_connect: conectar a un puerto remoto

Cuando un servicio hace `connect()` como **cliente**:

```
name_connect — el servicio CONECTA
====================================

  httpd_t  ──connect()──►  TCP 3306 (mysqld_port_t)    depende de boolean
  httpd_t  ──connect()──►  TCP 5432 (postgresql_port_t) depende de boolean

  Regla:
    allow httpd_t mysqld_port_t:tcp_socket name_connect;
    (habilitada por httpd_can_network_connect_db)
```

El AVC de un `name_connect` denegado:

```
type=AVC msg=audit(...): avc:  denied  { name_connect }
  for  pid=1234 comm="httpd"
  dest=3306                         ← puerto destino
  scontext=system_u:system_r:httpd_t:s0
  tcontext=system_u:object_r:mysqld_port_t:s0
  tclass=tcp_socket permissive=0
```

### Diferencia clave

```
┌──────────────────────────────────────────────────────┐
│            name_bind vs name_connect                 │
├──────────────┬───────────────────────────────────────┤
│ name_bind    │ Servicio ESCUCHA en ese puerto        │
│              │ Syscall: bind() + listen()            │
│              │ Solución: semanage port -a            │
├──────────────┼───────────────────────────────────────┤
│ name_connect │ Servicio CONECTA a ese puerto         │
│              │ Syscall: connect()                    │
│              │ Solución: setsebool (si existe) o     │
│              │           semanage port -a            │
└──────────────┴───────────────────────────────────────┘
```

> **Pregunta de predicción**: si cambias Apache de puerto 80 a 8080, ¿qué
> operación SELinux se denegará? ¿Necesitas un boolean o `semanage port`?
>
> **Respuesta**: `name_bind` (Apache escucha, no conecta). Pero 8080 ya es
> `http_cache_port_t`, y la política de Apache generalmente incluye ese tipo,
> así que podría funcionar sin cambios. Verifica con `semanage port -l | grep
> 8080`.

---

## 5. Agregar puertos: semanage port -a

### Sintaxis básica

```bash
semanage port -a -t TIPO -p PROTOCOLO PUERTO
#              │    │       │           │
#              │    │       │           └── Número de puerto o rango
#              │    │       └── tcp o udp
#              │    └── Tipo SELinux a asignar
#              └── -a = add (agregar)
```

### Ejemplo: Apache en puerto 8888

```bash
# 1. Verificar el tipo actual del puerto
semanage port -l | grep 8888
# (vacío o un tipo genérico)

# 2. Agregar el puerto como http_port_t
semanage port -a -t http_port_t -p tcp 8888

# 3. Verificar
semanage port -l | grep 8888
# http_port_t                  tcp      8888

# 4. Ver que aparece en las modificaciones locales
semanage port -l -C
# http_port_t                  tcp      8888
```

### Agregar un rango de puertos

```bash
# Agregar un rango completo
semanage port -a -t http_port_t -p tcp 8880-8890

# Verificar
semanage port -l | grep http_port_t
# http_port_t    tcp    80, 443, 488, 8008, 8009, 8443, 8880-8890
```

### Ejemplo: SSH en puerto alternativo

```bash
# Mover SSH al puerto 2222
# 1. Editar /etc/ssh/sshd_config
#    Port 2222

# 2. Agregar el puerto a ssh_port_t
semanage port -a -t ssh_port_t -p tcp 2222

# 3. Reiniciar sshd
systemctl restart sshd

# 4. Verificar
ss -Ztlnp | grep 2222
# LISTEN  0  128  *:2222  users:(("sshd",pid=...,proc_ctx=system_u:system_r:sshd_t:s0-s0:c0.c1023))
```

> **Importante**: si omites el paso 2, sshd (que corre como `sshd_t`) no podrá
> hacer `bind()` en el puerto 2222 porque ese puerto probablemente tiene un tipo
> que `sshd_t` no tiene permitido.

### Ejemplo: MariaDB en puerto no estándar

```bash
# MariaDB en puerto 3307 (segundo instancia)
semanage port -a -t mysqld_port_t -p tcp 3307

# Verificar
semanage port -l | grep mysqld
# mysqld_port_t                tcp      3306, 3307
```

### Puerto ya asignado a otro tipo

Si el puerto ya tiene un tipo asignado, `-a` falla:

```bash
semanage port -a -t http_port_t -p tcp 22
# ValueError: Port tcp/22 already defined

# El puerto 22 ya es ssh_port_t — no puedes agregar otro tipo con -a
# Necesitas -m (modify) para cambiar el tipo
```

---

## 6. Modificar y eliminar puertos

### Modificar: semanage port -m

Cambia el tipo de un puerto que **ya tiene asignación**:

```bash
# Sintaxis
semanage port -m -t NUEVO_TIPO -p PROTOCOLO PUERTO

# Ejemplo: reasignar puerto 8080 de http_cache_port_t a http_port_t
semanage port -m -t http_port_t -p tcp 8080
```

> **Precaución**: `-m` solo funciona con puertos que ya tienen un tipo
> asignado. Para puertos sin asignación, usa `-a`.

### Cuándo usar -a vs -m

```
¿El puerto ya tiene un tipo asignado?
│
├── NO → semanage port -a -t tipo -p proto puerto
│        (agrega nueva asignación)
│
└── SÍ → ¿Es el tipo que necesitas?
          │
          ├── SÍ → No necesitas hacer nada
          │
          └── NO → semanage port -m -t tipo -p proto puerto
                   (modifica la asignación existente)
```

### Eliminar: semanage port -d

Solo puedes eliminar asignaciones **locales** (las que añadiste con `-a`):

```bash
# Eliminar una asignación local
semanage port -d -t http_port_t -p tcp 8888

# Verificar que se eliminó
semanage port -l -C
# (ya no aparece 8888)
```

No puedes eliminar asignaciones de la política base:

```bash
# Intentar eliminar puerto 80 de http_port_t
semanage port -d -t http_port_t -p tcp 80
# ValueError: Port tcp/80 is defined in policy, cannot be deleted

# Las asignaciones de la política base son inmutables
# Solo puedes modificarlas con -m (pero ten cuidado)
```

### Resumen de operaciones

```
┌─────────────────────────────────────────────────────────────┐
│           Operaciones semanage port                         │
├────────┬────────────────────────────────────────────────────┤
│ -l     │ Listar todos los puertos etiquetados              │
│ -l -C  │ Listar solo modificaciones locales                │
│ -a     │ Agregar puerto (falla si ya existe tipo)          │
│ -m     │ Modificar tipo de puerto existente                │
│ -d     │ Eliminar asignación local (no base)               │
├────────┼────────────────────────────────────────────────────┤
│ -t     │ Especificar tipo SELinux                          │
│ -p     │ Especificar protocolo (tcp/udp)                   │
└────────┴────────────────────────────────────────────────────┘
```

---

## 7. Escenarios prácticos

### Escenario 1: Nginx en puerto 3000 (aplicación Node.js proxied)

```bash
# Síntoma: Nginx no puede escuchar en puerto 3000
# Error en journalctl:
#   nginx: [emerg] bind() to 0.0.0.0:3000 failed (13: Permission denied)

# 1. Confirmar que es SELinux
ausearch -m AVC -ts recent
# type=AVC denied { name_bind } for pid=... comm="nginx"
#   src=3000 scontext=...httpd_t... tcontext=...reserved_port_t...

# 2. Verificar tipo actual del puerto
semanage port -l | grep 3000
# (vacío o un tipo no-http)

# 3. Agregar el puerto
semanage port -a -t http_port_t -p tcp 3000

# 4. Reiniciar Nginx
systemctl restart nginx

# 5. Verificar
ss -Ztlnp | grep 3000
# LISTEN ... nginx ... httpd_t ...
```

### Escenario 2: Nginx como proxy reverso conecta a backend en puerto 5000

```bash
# Síntoma: Nginx devuelve 502 Bad Gateway
# El backend (Gunicorn/Flask) está corriendo en 5000

# 1. Buscar AVC
ausearch -m AVC -c nginx -ts recent
# type=AVC denied { name_connect } for pid=... comm="nginx"
#   dest=5000 scontext=...httpd_t... tcontext=...commplex_main_port_t...

# 2. Dos soluciones posibles:

# Solución A: Boolean (permite conectar a CUALQUIER puerto)
setsebool -P httpd_can_network_connect on

# Solución B: Más granular — agregar 5000 como http_port_t
semanage port -a -t http_port_t -p tcp 5000
# Ahora httpd_t puede conectar a puertos http_port_t sin boolean genérico

# La solución B es más restrictiva (mejor seguridad)
# La solución A es más flexible (no necesitas agregar cada puerto)
```

### Escenario 3: Tomcat en puertos 8180 y 8543

```bash
# Tomcat AJP + HTTPS en puertos no estándar

# 1. Verificar qué tipo necesita Tomcat
ps -eZ | grep tomcat
# system_u:system_r:tomcat_t:s0    ... java

# 2. Verificar qué puertos permite tomcat_t
# (tomcat_t generalmente puede usar http_port_t)
sesearch -A -s tomcat_t -c tcp_socket -p name_bind
# allow tomcat_t http_port_t:tcp_socket name_bind;

# 3. Agregar los puertos como http_port_t
semanage port -a -t http_port_t -p tcp 8180
semanage port -a -t http_port_t -p tcp 8543

# 4. Reiniciar Tomcat
systemctl restart tomcat
```

### Escenario 4: Servicio personalizado con puerto UDP

```bash
# Un servicio de monitorización custom usa UDP 9995

# 1. El servicio corre en un dominio custom (monitoring_t)
# 2. Verificar qué tipo de puerto puede usar
sesearch -A -s monitoring_t -c udp_socket -p name_bind

# 3. Si usa un tipo genérico como unreserved_port_t:
semanage port -l | grep 9995
# (verificar si ya está en unreserved_port_t)

# Si 9995 no está en el rango unreserved (61000-65535),
# necesitas asignarle el tipo correcto:
semanage port -a -t monitoring_port_t -p udp 9995
```

### Escenario 5: Múltiples instancias del mismo servicio

```bash
# Tres instancias de PostgreSQL en puertos 5432, 5433, 5434

# Puerto por defecto ya está asignado
semanage port -l | grep postgresql
# postgresql_port_t            tcp      5432

# Agregar los puertos adicionales
semanage port -a -t postgresql_port_t -p tcp 5433
semanage port -a -t postgresql_port_t -p tcp 5434

# Verificar
semanage port -l | grep postgresql
# postgresql_port_t            tcp      5432-5434

# Las tres instancias pueden arrancar sin problemas
```

---

## 8. Integración con el flujo de troubleshooting

Los puertos son el **tercer paso** en el flujo de diagnóstico SELinux que vimos
en T03:

```
Flujo de troubleshooting SELinux
=================================

  AVC denied
      │
      ▼
  audit2why
      │
      ├── "Boolean X is incorrect" ──────► setsebool -P X on
      │
      ├── "Missing TE rule" ──────────┐
      │                               │
      │   ¿Qué operación?             │
      │   │                           │
      │   ├── {read}/{write}/{open}   │
      │   │   → File context          │
      │   │   → semanage fcontext     │
      │   │   → restorecon            │
      │   │                           │
      │   ├── {name_bind}       ◄─────┘
      │   │   → Puerto incorrecto
      │   │   → semanage port -a
      │   │
      │   ├── {name_connect}
      │   │   → Boolean (httpd_can_network_connect_db)
      │   │   → o semanage port -a
      │   │
      │   └── Otro
      │       → audit2allow (último recurso)
      │
      └── (otros casos)
```

### Identificar problemas de puertos en AVC

Las claves para reconocer un problema de puertos:

```
Indicadores de problema de puerto en AVC
==========================================

  1. Operación = name_bind o name_connect
  2. tclass = tcp_socket o udp_socket
  3. tcontext contiene un tipo de puerto (*_port_t)
  4. Campos src= (bind) o dest= (connect) muestran el número de puerto

  Ejemplo name_bind:
    denied { name_bind } ... src=9090 ... tcontext=...reserved_port_t...
    ─────────────────         ────        ───────────────────────────
    operación                 puerto      tipo actual del puerto

  Ejemplo name_connect:
    denied { name_connect } ... dest=6379 ... tcontext=...redis_port_t...
    ───────────────────          ────          ──────────────────────
    operación                    destino       tipo del puerto destino
```

### audit2why con problemas de puertos

```bash
# audit2why te indica si el problema es de puerto
ausearch -m AVC -ts recent --raw | audit2why

# Salida típica:
#   Was caused by:
#   The boolean httpd_can_network_connect was set incorrectly.
#   Allow httpd to connect to all ports.
#
#   Allow access by executing:
#   setsebool -P httpd_can_network_connect 1

# O bien:
#   Was caused by:
#   Missing type enforcement (TE) allow rule.
#   You can use audit2allow to generate a loadable module.
```

Si `audit2why` sugiere `audit2allow`, antes de generar un módulo custom,
verifica si `semanage port -a` resuelve el problema. Es casi siempre la
solución correcta para `name_bind`.

---

## 9. Errores comunes

### Error 1: olvidar el protocolo (-p)

```bash
# MAL — falta -p
semanage port -a -t http_port_t 8888
# usage: semanage port [-h] ...

# BIEN
semanage port -a -t http_port_t -p tcp 8888

# TCP y UDP son independientes — el mismo puerto puede tener
# tipos diferentes en cada protocolo:
semanage port -a -t http_port_t -p tcp 8888
semanage port -a -t dns_port_t -p udp 8888
# Ambos son válidos y coexisten
```

### Error 2: usar -a cuando el puerto ya tiene tipo

```bash
# Puerto 8080 ya es http_cache_port_t
semanage port -a -t http_port_t -p tcp 8080
# ValueError: Port tcp/8080 already defined

# Solución: usar -m para modificar
semanage port -m -t http_port_t -p tcp 8080
```

### Error 3: no verificar si el tipo es el correcto para el dominio

```bash
# Agregar puerto para httpd pero con tipo incorrecto
semanage port -a -t mysqld_port_t -p tcp 9090

# httpd_t no tiene permiso name_bind sobre mysqld_port_t
# → Sigue dando AVC denied

# Hay que usar el tipo que el dominio tiene permitido:
semanage port -d -t mysqld_port_t -p tcp 9090
semanage port -a -t http_port_t -p tcp 9090
```

Para saber qué tipo usar, consulta qué permite el dominio:

```bash
# ¿Sobre qué tipos de puerto puede hacer name_bind httpd_t?
sesearch -A -s httpd_t -c tcp_socket -p name_bind
# allow httpd_t http_port_t:tcp_socket name_bind;
# → Necesitas usar http_port_t
```

### Error 4: confundir name_bind con name_connect

```bash
# Síntoma: httpd no puede conectar al backend en puerto 5000
# Diagnóstico incorrecto: agregar 5000 como http_port_t para name_bind
# El problema es name_connect, no name_bind

# Para name_connect hay que verificar si existe un boolean:
audit2why  # Si dice "boolean httpd_can_network_connect" → usar boolean
           # Si no hay boolean → semanage port -a puede ayudar
           # dependiendo de la política
```

### Error 5: no considerar el firewall junto con SELinux

```bash
# Has agregado el puerto en SELinux pero el servicio sigue sin funcionar

# SELinux controla qué puede hacer el PROCESO
# El firewall controla qué tráfico LLEGA al proceso

# Lista de verificación completa para un puerto no estándar:
# 1. Configurar el servicio (ej: Listen 8888 en httpd.conf)
# 2. SELinux: semanage port -a -t http_port_t -p tcp 8888
# 3. Firewall: firewall-cmd --add-port=8888/tcp --permanent
#              firewall-cmd --reload
# 4. Reiniciar servicio: systemctl restart httpd

# Olvidar cualquiera de los tres causa "no funciona"
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║                    SELinux Ports — Cheatsheet                   ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  CONSULTAR                                                       ║
║  semanage port -l                  Todos los puertos             ║
║  semanage port -l | grep http      Filtrar por servicio          ║
║  semanage port -l | grep -w 8080   Buscar puerto específico     ║
║  semanage port -l -C               Solo cambios locales          ║
║                                                                  ║
║  AGREGAR                                                         ║
║  semanage port -a -t TYPE -p tcp PORT   Agregar puerto           ║
║  semanage port -a -t TYPE -p tcp P1-P2  Agregar rango            ║
║                                                                  ║
║  MODIFICAR                                                       ║
║  semanage port -m -t TYPE -p tcp PORT   Cambiar tipo existente   ║
║                                                                  ║
║  ELIMINAR                                                        ║
║  semanage port -d -t TYPE -p tcp PORT   Eliminar (solo locales)  ║
║                                                                  ║
║  TIPOS COMUNES                                                   ║
║  http_port_t       → 80, 443, 8008, 8009, 8443                  ║
║  ssh_port_t        → 22                                          ║
║  mysqld_port_t     → 3306                                        ║
║  postgresql_port_t → 5432                                        ║
║  dns_port_t        → 53                                          ║
║  smtp_port_t       → 25, 465, 587                                ║
║                                                                  ║
║  DIAGNÓSTICO                                                     ║
║  ausearch -m AVC -ts recent         Ver AVC recientes            ║
║  ausearch ... --raw | audit2why     Entender la causa            ║
║  sesearch -A -s DOMAIN -c tcp_socket -p name_bind                ║
║    → Qué tipos de puerto puede usar un dominio                   ║
║                                                                  ║
║  FLUJO PARA PUERTO NO ESTÁNDAR                                   ║
║  1. Configurar servicio (ej: Port 2222 en sshd_config)           ║
║  2. semanage port -a -t ssh_port_t -p tcp 2222                   ║
║  3. firewall-cmd --add-port=2222/tcp --permanent && reload       ║
║  4. systemctl restart sshd                                       ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Apache en múltiples puertos

**Objetivo**: configurar Apache para escuchar en los puertos 80, 8080 y 9443.

```bash
# Paso 1: Verificar qué puertos ya son http_port_t
semanage port -l | grep http_port_t
```

> **Pregunta de predicción**: de los tres puertos (80, 8080, 9443), ¿cuáles
> crees que ya están asignados a un tipo que Apache puede usar y cuáles
> necesitan `semanage port -a`?
>
> **Respuesta**: el puerto 80 ya es `http_port_t` (no necesita cambios). El
> puerto 8080 es `http_cache_port_t` — httpd_t generalmente tiene permitido
> este tipo también, así que probablemente funcione sin cambios (verifica con
> `sesearch`). El puerto 9443 probablemente necesita `semanage port -a -t
> http_port_t -p tcp 9443`.

```bash
# Paso 2: Agregar el puerto que falta
semanage port -a -t http_port_t -p tcp 9443

# Paso 3: Configurar Apache (/etc/httpd/conf/httpd.conf)
# Listen 80
# Listen 8080
# Listen 9443

# Paso 4: Reiniciar y verificar
systemctl restart httpd
ss -Ztlnp | grep httpd
```

### Ejercicio 2: Diagnóstico de puerto denegado

**Escenario**: un administrador instaló Redis y lo configuró para escuchar en
el puerto 6380 (segundo instancia). Redis no arranca.

```bash
# Paso 1: Verificar el error
systemctl status redis-secondary
# redis-secondary.service: Failed ...
journalctl -u redis-secondary -n 20
# Could not create server TCP listening socket *:6380: bind: Permission denied

# Paso 2: Buscar AVC
ausearch -m AVC -ts recent -c redis-server
```

> **Pregunta de predicción**: ¿qué campos del AVC te confirmarán que es un
> problema de puerto? ¿Qué tipo tendrá el `tcontext`?
>
> **Respuesta**: la operación será `{ name_bind }`, el campo `src=6380`, y el
> `tcontext` contendrá el tipo actual del puerto 6380 (probablemente
> `unreserved_port_t` o `reserved_port_t`, no `redis_port_t`). El `scontext`
> mostrará `redis_t`.

```bash
# Paso 3: Resolver
semanage port -a -t redis_port_t -p tcp 6380

# Paso 4: Reiniciar
systemctl restart redis-secondary

# Paso 5: Verificar
semanage port -l | grep redis
# redis_port_t                 tcp      6379, 6380
```

### Ejercicio 3: Auditoría de puertos modificados

**Objetivo**: crear un script de auditoría que muestre todos los cambios de
puertos respecto a la política base.

```bash
# Paso 1: Listar cambios locales
semanage port -l -C

# Paso 2: Para cada puerto modificado, verificar qué servicio lo usa
# Ejemplo manual:
semanage port -l -C
# http_port_t    tcp    8888, 9443
# ssh_port_t     tcp    2222

# Verificar qué procesos escuchan en esos puertos
ss -Ztlnp | grep -E '8888|9443|2222'
```

> **Pregunta de predicción**: si ejecutas `semanage port -l -C` en un servidor
> recién instalado (sin modificaciones), ¿qué salida esperas?
>
> **Respuesta**: salida vacía. El flag `-C` solo muestra las modificaciones
> hechas por el administrador con `semanage port -a` o `-m`. La política base
> (que define los puertos por defecto como 80→http_port_t) no aparece.

---

> **Sección completada**: S02 — Gestión (Booleans, File contexts,
> Troubleshooting, Ports).
>
> **Siguiente tema**: C02 — AppArmor, S01 — Fundamentos, T01 — Perfiles
> (enforce vs complain, aa-status).
