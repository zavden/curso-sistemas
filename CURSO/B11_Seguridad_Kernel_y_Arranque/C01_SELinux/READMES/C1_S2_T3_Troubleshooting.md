# Troubleshooting SELinux — audit2why, audit2allow, sealert

## Índice

1. [El flujo de troubleshooting](#1-el-flujo-de-troubleshooting)
2. [/var/log/audit/audit.log — la fuente de verdad](#2-varlogauditauditlog--la-fuente-de-verdad)
3. [ausearch — buscar AVC denials](#3-ausearch--buscar-avc-denials)
4. [audit2why — entender por qué se denegó](#4-audit2why--entender-por-qué-se-denegó)
5. [sealert — diagnóstico amigable](#5-sealert--diagnóstico-amigable)
6. [audit2allow — generar política personalizada](#6-audit2allow--generar-política-personalizada)
7. [dontaudit y AVC ocultos](#7-dontaudit-y-avc-ocultos)
8. [Escenarios de troubleshooting completos](#8-escenarios-de-troubleshooting-completos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El flujo de troubleshooting

Cuando algo falla y sospechas de SELinux, sigue este flujo ordenado:

```
Servicio falla
│
├── 1. ¿Es SELinux?
│   setenforce 0 → ¿funciona? → No → no es SELinux → buscar en otro lado
│                              → Sí → es SELinux → setenforce 1 → continuar
│
├── 2. ¿Qué deniega?
│   ausearch -m AVC -ts recent
│   → Lee el mensaje AVC
│
├── 3. ¿Por qué deniega?
│   ausearch -m AVC -ts recent | audit2why
│   │
│   ├── "Boolean XXX was set incorrectly"
│   │   → setsebool -P XXX on → RESUELTO
│   │
│   ├── "Missing type enforcement rule"
│   │   → ¿Es un file context incorrecto?
│   │     ls -Z archivo → matchpathcon archivo
│   │     → Si difieren: restorecon -Rv → RESUELTO
│   │     → Si la ruta no tiene regla: semanage fcontext -a → RESUELTO
│   │
│   ├── "Missing type enforcement rule" (y no es file context)
│   │   → ¿Es un puerto?
│   │     semanage port -l | grep PUERTO
│   │     → semanage port -a -t tipo -p tcp PUERTO → RESUELTO
│   │
│   └── Nada de lo anterior funciona
│       → audit2allow → generar política personalizada
│       → ÚLTIMA opción, evaluar con cuidado
│
└── 4. Verificar
    → Reproducir la operación
    → ausearch -m AVC -ts recent (no debe haber nuevos AVCs)
```

> **Regla de oro**: Intentar soluciones en este orden: boolean → file context → port → política personalizada. Cada paso es más permisivo que el anterior.

---

## 2. /var/log/audit/audit.log — la fuente de verdad

Cuando SELinux deniega una operación, genera un mensaje **AVC** (*Access Vector Cache*) en el log de auditoría.

### Ubicación del log

```bash
# Log principal (siempre existe si auditd corre)
/var/log/audit/audit.log

# Si auditd no corre, los AVC van a:
/var/log/messages    (vía syslog)
# O:
journalctl           (vía systemd journal)
```

### Anatomía de un mensaje AVC

```
type=AVC msg=audit(1711008000.123:456): avc:  denied  { read } for  pid=1234 comm="httpd" name="index.html" dev="sda1" ino=12345 scontext=system_u:system_r:httpd_t:s0 tcontext=system_u:object_r:user_home_t:s0 tclass=file permissive=0
```

Desglose campo por campo:

```
type=AVC                          tipo de mensaje: Access Vector Cache
msg=audit(1711008000.123:456)     timestamp Unix : número de serie
avc: denied                       resultado: denegado
{ read }                          operación denegada (read, write, open, etc.)
pid=1234                          PID del proceso que intentó la operación
comm="httpd"                      nombre del comando
name="index.html"                 nombre del archivo objetivo
dev="sda1"                        dispositivo donde está el archivo
ino=12345                         inodo del archivo
scontext=system_u:system_r:httpd_t:s0     contexto del PROCESO (source)
tcontext=system_u:object_r:user_home_t:s0 contexto del OBJETIVO (target)
tclass=file                       clase del objeto (file, dir, tcp_socket...)
permissive=0                      0=enforcing (se denegó), 1=permissive (solo log)
```

### Leer un AVC rápidamente

```
La información esencial está en 4 campos:

  { read }          ← QUÉ operación
  comm="httpd"      ← QUIÉN lo intentó
  scontext=...httpd_t...     ← dominio del proceso
  tcontext=...user_home_t... ← tipo del objetivo

  Traducción humana:
  "Apache (httpd_t) intentó LEER un archivo de tipo user_home_t
   y fue DENEGADO"
```

### Tipos de operaciones comunes

| Operación | Descripción | Ejemplo |
|---|---|---|
| `{ read }` | Leer archivo | httpd lee un HTML |
| `{ write }` | Escribir archivo | httpd escribe un log |
| `{ open }` | Abrir archivo | Precede a read/write |
| `{ getattr }` | Leer atributos (stat) | ls, find |
| `{ execute }` | Ejecutar archivo | exec un binario |
| `{ name_bind }` | Bind a un puerto | httpd escucha en 9090 |
| `{ name_connect }` | Conectar a un puerto | httpd conecta a MySQL 3306 |
| `{ create }` | Crear archivo/socket | Crear archivo nuevo |
| `{ unlink }` | Eliminar archivo | rm |
| `{ search }` | Buscar en directorio | Acceder a un subdirectorio |
| `{ connectto }` | Conectar a socket Unix | Conectar a PHP-FPM |

### Tipos de clases de objeto (tclass)

| Clase | Descripción |
|---|---|
| `file` | Archivo regular |
| `dir` | Directorio |
| `lnk_file` | Enlace simbólico |
| `tcp_socket` | Socket TCP |
| `udp_socket` | Socket UDP |
| `unix_stream_socket` | Socket Unix stream |
| `process` | Proceso (transición, señal) |
| `capability` | Capacidad Linux (CAP_NET_BIND_SERVICE, etc.) |

---

## 3. ausearch — buscar AVC denials

`ausearch` busca en el log de auditoría de forma estructurada.

### Búsquedas básicas

```bash
# Todos los AVC denials recientes (últimos 10 minutos)
ausearch -m AVC -ts recent

# AVC denials de hoy
ausearch -m AVC -ts today

# AVC denials de los últimos 30 minutos
ausearch -m AVC -ts $(date -d '30 minutes ago' '+%H:%M:%S')

# AVC denials desde una hora específica
ausearch -m AVC -ts 14:30:00

# AVC denials de un día específico
ausearch -m AVC -ts 03/21/2026

# Todos los AVC (sin filtro de tiempo)
ausearch -m AVC
```

### Filtrar por proceso o contexto

```bash
# Solo AVC de httpd
ausearch -m AVC -c httpd

# Solo AVC de un PID específico
ausearch -m AVC -p 1234

# Solo AVC del dominio httpd_t
ausearch -m AVC | grep httpd_t

# Solo AVC que involucren un tipo de archivo específico
ausearch -m AVC | grep shadow_t
```

### Formato de salida

```bash
# Formato raw (para pasar a audit2why)
ausearch -m AVC -ts recent --raw
# Líneas tipo=AVC sin formato adicional

# Formato interpretado (más legible)
ausearch -m AVC -ts recent -i
# Traduce UIDs a nombres, timestamps a fechas legibles
# type=AVC ... scontext=system_u:system_r:httpd_t:s0 ...
# → con -i muestra nombres resueltos

# Solo la primera coincidencia
ausearch -m AVC -ts recent --just-one
```

### Combinar con audit2why

```bash
# El comando más útil para troubleshooting SELinux:
ausearch -m AVC -ts recent | audit2why

# Para un servicio específico:
ausearch -m AVC -c httpd -ts recent | audit2why

# Si no hay resultados recientes, ampliar el rango:
ausearch -m AVC -ts today | audit2why
```

---

## 4. audit2why — entender por qué se denegó

`audit2why` lee mensajes AVC y explica la **causa** y la **solución sugerida**.

### Tipos de respuesta

#### Respuesta 1: Boolean incorrecto

```bash
$ ausearch -m AVC -ts recent | audit2why

type=AVC msg=audit(1711008000.123:456): avc: denied { name_connect }
  for pid=1234 comm="httpd" dest=3306
  scontext=system_u:system_r:httpd_t:s0
  tcontext=system_u:object_r:mysqld_port_t:s0 tclass=tcp_socket

    Was caused by:
    The boolean httpd_can_network_connect_db was set incorrectly.
    Description:
    Allow httpd to can network connect db

    Allow access by executing:
    # setsebool -P httpd_can_network_connect_db 1
```

Traducción: "httpd intentó conectar al puerto 3306 (MySQL). El boolean `httpd_can_network_connect_db` está en off. Actívalo para permitir."

Solución: `setsebool -P httpd_can_network_connect_db on`

#### Respuesta 2: Falta regla de Type Enforcement

```bash
$ ausearch -m AVC -ts recent | audit2why

type=AVC msg=audit(1711008000.789:789): avc: denied { read }
  for pid=1234 comm="httpd" name="data.json"
  scontext=system_u:system_r:httpd_t:s0
  tcontext=system_u:object_r:default_t:s0 tclass=file

    Was caused by:
        Missing type enforcement (TE) allow rule.

        You can use audit2allow to generate a loadable module to allow this access.
```

Traducción: "httpd intentó leer un archivo de tipo `default_t`. No hay ninguna regla que permita esto ni boolean que lo controle."

Diagnóstico: El archivo probablemente tiene tipo incorrecto. Verificar:

```bash
ls -Z /ruta/data.json
# ...default_t...    ← ¿debería ser httpd_sys_content_t?

matchpathcon /ruta/data.json
# ¿Qué tipo debería tener según su ruta?
```

#### Respuesta 3: Dominio no confinado pero restricción

```bash
    Was caused by:
        The source type unconfined_t was not allowed to ...
        Possible cause: the source is unconfined but the target is protected.
```

Esto ocurre raramente en targeted — incluso `unconfined_t` tiene algunas restricciones sobre tipos ultra-protegidos.

### audit2why sin ausearch

```bash
# Pasar un archivo de AVCs directamente
audit2why < /var/log/audit/audit.log

# Solo la última hora
grep "type=AVC" /var/log/audit/audit.log | tail -20 | audit2why
```

---

## 5. sealert — diagnóstico amigable

`sealert` es parte de `setroubleshoot` y proporciona diagnósticos mucho más legibles que `audit2why`, con explicaciones detalladas y múltiples sugerencias de solución.

### Instalación

```bash
# RHEL/Fedora
dnf install setroubleshoot-server

# Habilitar el servicio
systemctl enable --now setroubleshootd
```

### Uso

```bash
# Ver todas las alertas SELinux
sealert -a /var/log/audit/audit.log

# Ver solo las últimas
sealert -a /var/log/audit/audit.log | tail -80
```

### Ejemplo de salida de sealert

```
SELinux is preventing httpd from read access on the file data.json.

*****  Plugin restorecon (99.5 confidence) suggests   ************************

If you want to fix the label.
/srv/web/data.json default label should be httpd_sys_content_t.
Then you can run restorecon. The access attempt may have been stopped
due to insufficient permissions to access a parent directory, in which
case try to change the following command accordingly.
Do
# /sbin/restorecon -v /srv/web/data.json

*****  Plugin catchall_boolean (7.83 confidence) suggests  *******************

If you want to allow httpd to read user content
Then you must tell SELinux about this by enabling the
'httpd_read_user_content' boolean.
Do
# setsebool -P httpd_read_user_content 1

*****  Plugin catchall (1.41 confidence) suggests  ***************************

If you believe that httpd should be allowed read access on the data.json file
by default.
Then you should report this as a bug.
You can generate a local policy module to allow this access.
Do
# ausearch -c 'httpd' --raw | audit2allow -M my-httpd
# semodule -i my-httpd.pp

Additional Information:
Source Context:     system_u:system_r:httpd_t:s0
Target Context:     unconfined_u:object_r:default_t:s0
Target Objects:     data.json [ file ]
Source:             httpd
...
```

### Interpretar la salida

```
sealert ofrece soluciones ORDENADAS por confianza:

  Plugin restorecon (99.5 confidence)
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  Casi seguro: el archivo tiene tipo incorrecto
  → restorecon -v /srv/web/data.json

  Plugin catchall_boolean (7.83 confidence)
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  Poco probable pero posible: un boolean podría resolverlo
  → setsebool -P httpd_read_user_content 1

  Plugin catchall (1.41 confidence)
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  Última opción: generar política personalizada
  → audit2allow

Empezar SIEMPRE por la de mayor confianza.
```

### sealert desde el journal

```bash
# Si setroubleshootd está corriendo, los mensajes van al journal
journalctl -t setroubleshoot --since "1 hour ago"

# Formato típico:
# SELinux is preventing httpd from read access on the file ...
# For complete SELinux messages run: sealert -l UUID
sealert -l 12345678-1234-1234-1234-123456789abc
# Muestra el diagnóstico completo para ese UUID
```

---

## 6. audit2allow — generar política personalizada

`audit2allow` es el **último recurso** cuando ni booleans ni file contexts ni ports resuelven el problema. Genera módulos de política personalizados a partir de AVCs denegados.

### Cuándo usarlo

```
SOLO cuando:
  1. audit2why dice "Missing type enforcement (TE) allow rule"
  2. El tipo del archivo ES correcto (no es un problema de file context)
  3. No hay boolean que resuelva el caso
  4. No es un problema de puertos
  5. La operación es legítima (no un intento de ataque)
```

### Paso 1: Ver qué regla se generaría

```bash
# Primero ver (sin aplicar) qué regla propone
ausearch -m AVC -ts recent | audit2allow
# #============= httpd_t ==============
# allow httpd_t var_t:file { read open getattr };

# Esto muestra la regla TE que se añadiría
# EVALÚA si tiene sentido antes de aplicar
```

### Paso 2: Generar el módulo

```bash
# Generar un módulo compilado
ausearch -m AVC -ts recent | audit2allow -M my_httpd_policy
# ******************** IMPORTANT ***********************
# To make this policy package active, execute:
# semodule -i my_httpd_policy.pp

# Esto crea dos archivos:
# my_httpd_policy.te   ← fuente de la política (texto legible)
# my_httpd_policy.pp   ← módulo compilado (binario)
```

### Paso 3: Revisar la política antes de instalar

```bash
# SIEMPRE revisar el .te antes de instalar
cat my_httpd_policy.te
# module my_httpd_policy 1.0;
#
# require {
#     type httpd_t;
#     type var_t;
#     class file { read open getattr };
# }
#
# #============= httpd_t ==============
# allow httpd_t var_t:file { read open getattr };

# Pregúntate:
# ¿Es razonable que httpd lea archivos de tipo var_t?
# ¿O debería el archivo tener un tipo más específico?
# Si var_t es muy genérico → probablemente el archivo debería
# tener httpd_sys_content_t → solución es file context, no política
```

### Paso 4: Instalar el módulo

```bash
# Solo si la revisión confirma que es correcto
semodule -i my_httpd_policy.pp

# Verificar que está cargado
semodule -l | grep my_httpd_policy
# my_httpd_policy

# Si necesitas revertir:
semodule -r my_httpd_policy
```

### Generar política en modo permissive

Para una aplicación nueva que genera muchos AVCs, el flujo es:

```bash
# 1. Poner el dominio en permissive
semanage permissive -a myapp_t

# 2. Ejecutar TODA la funcionalidad de la aplicación
#    (login, operaciones normales, edge cases)
#    Esto genera todos los AVC que la app necesita

# 3. Generar política acumulada
ausearch -m AVC -ts today | audit2allow -M myapp_policy
cat myapp_policy.te    # REVISAR

# 4. Instalar
semodule -i myapp_policy.pp

# 5. Quitar permissive
semanage permissive -d myapp_t

# 6. Verificar que todo funciona en enforcing
```

### Peligros de audit2allow

```
⚠ audit2allow genera la regla MÍNIMA para permitir lo que se denegó.
  Pero puede generar reglas demasiado amplias si no tienes cuidado:

  MAL:
  allow httpd_t file_type:file { read write execute };
  → Permite a Apache leer, escribir y EJECUTAR CUALQUIER archivo
  → Esto anula el propósito de SELinux

  Señales de alarma en las reglas generadas:
  - allow ... unconfined_t ...     ← demasiado amplio
  - allow ... file_type ...        ← categoría genérica
  - ... { read write execute } ... ← demasiados permisos
  - ... permissive httpd_t ...     ← NUNCA (deja httpd sin confinar)
```

---

## 7. dontaudit y AVC ocultos

Algunas operaciones denegadas no generan AVC en el log porque la política las marca con `dontaudit`. Esto reduce ruido en los logs pero puede complicar el debugging.

### ¿Qué es dontaudit?

```
allow:     permite la operación + no genera log
deny:      deniega la operación + GENERA log (AVC)
dontaudit: deniega la operación + NO genera log (silencioso)

dontaudit se usa para operaciones que:
  - Se deniegan intencionalmente
  - Son inofensivas (el proceso maneja el error gracefully)
  - Generarían miles de AVCs ruidosos sin valor
```

### Ejemplo

```
Muchos procesos intentan leer /proc/self/status con getattr.
SELinux deniega pero el proceso continúa sin problemas.
Sin dontaudit: miles de AVCs en el log por minuto.
Con dontaudit: silencio.
```

### Desactivar dontaudit para debug

```bash
# Si sospechas que hay denegaciones ocultas:

# 1. Desactivar TODAS las reglas dontaudit
semodule -DB
# -D: deshabilita dontaudit
# -B: reconstruye la política

# 2. Reproducir el problema

# 3. Buscar AVCs (ahora incluye los que estaban ocultos)
ausearch -m AVC -ts recent | audit2why

# 4. RESTAURAR dontaudit inmediatamente (genera MUCHO ruido)
semodule -B
# Restaura las reglas dontaudit
```

> **Siempre restaurar** con `semodule -B` después de debuggear. Dejar dontaudit desactivado llena el log de auditoría rápidamente y puede impactar el rendimiento.

---

## 8. Escenarios de troubleshooting completos

### Escenario 1: Apache no puede servir archivos

```bash
# Síntoma: HTTP 403 Forbidden

# Paso 1: ¿Es SELinux?
setenforce 0
curl http://localhost/page.html    # funciona → es SELinux
setenforce 1

# Paso 2: Buscar AVC
ausearch -m AVC -c httpd -ts recent
# type=AVC ... denied { read } ... comm="httpd"
# scontext=...httpd_t... tcontext=...user_home_t... tclass=file

# Paso 3: Entender
ausearch -m AVC -c httpd -ts recent | audit2why
# "Missing type enforcement rule"

# Paso 4: Verificar contexto del archivo
ls -Z /var/www/html/page.html
# ...user_home_t...    ← tipo incorrecto

matchpathcon /var/www/html/page.html
# ...httpd_sys_content_t    ← tipo esperado

# Paso 5: Solución
restorecon -Rv /var/www/html/
# Relabeled from user_home_t to httpd_sys_content_t

# Paso 6: Verificar
curl http://localhost/page.html    # funciona ✓
ausearch -m AVC -c httpd -ts recent    # sin nuevos AVC ✓
```

### Escenario 2: Apache no conecta a la base de datos

```bash
# Síntoma: "Error establishing a database connection" en WordPress

# Paso 1: Buscar AVC
ausearch -m AVC -c httpd -ts recent | audit2why
# type=AVC ... denied { name_connect } ... dest=3306
# scontext=...httpd_t... tcontext=...mysqld_port_t...
#
# Was caused by:
# The boolean httpd_can_network_connect_db was set incorrectly.
# setsebool -P httpd_can_network_connect_db 1

# Paso 2: Solución
setsebool -P httpd_can_network_connect_db on

# Paso 3: Verificar
curl http://localhost/    # WordPress carga ✓
```

### Escenario 3: Servicio personalizado no arranca

```bash
# Síntoma: /opt/myapp/server no puede escuchar en puerto 8443

# Paso 1: Buscar AVC
ausearch -m AVC -ts recent | audit2why
# type=AVC ... denied { name_bind } ... src=8443
# scontext=...unconfined_service_t... tcontext=...unreserved_port_t...
#
# Missing type enforcement rule

# Paso 2: Verificar si el puerto tiene tipo
semanage port -l | grep 8443
# http_port_t    tcp    80, 81, 443, ..., 8443
# ¡El puerto SÍ tiene tipo http_port_t!
# Pero unconfined_service_t no es httpd_t

# El proceso corre como unconfined_service_t, no como un dominio
# que tiene permiso para http_port_t

# Opción A: Si la app debería usar httpd_t
# → Crear un tipo exec para que transicione

# Opción B: Añadir el puerto a un tipo que unconfined puede usar
# (o crear política personalizada)

# Opción C (rápida): El puerto 8443 ya está en http_port_t
# Si la app tiene su propio dominio, necesita permiso para ese tipo
# Si no tiene dominio propio (unconfined_service_t), crear política:
ausearch -m AVC -ts recent | audit2allow -M myapp_port
cat myapp_port.te    # revisar
semodule -i myapp_port.pp
```

### Escenario 4: No hay AVC pero algo falla

```bash
# Síntoma: servicio falla, parece SELinux pero ausearch no devuelve nada

# Posibilidad 1: dontaudit oculta los AVCs
semodule -DB                            # desactivar dontaudit
systemctl restart myservice             # reproducir
ausearch -m AVC -ts recent | audit2why  # buscar
semodule -B                             # restaurar dontaudit

# Posibilidad 2: el servicio está en permissive (log pero no deniega)
semanage permissive -l
# Si el servicio aparece aquí, los AVC se logean pero permissive=1

# Posibilidad 3: no es SELinux
getenforce
# Si dice Permissive o Disabled → no es SELinux
# Si dice Enforcing → verificar que setenforce 0 cambia el resultado
```

---

## 9. Errores comunes

### Error 1: Usar audit2allow sin diagnosticar primero

```bash
# MAL — ir directamente a audit2allow
ausearch -m AVC -ts today | audit2allow -M fix_everything
semodule -i fix_everything.pp
# Resultado: política demasiado permisiva que puede incluir
# reglas para archivos con tipo incorrecto

# Ejemplo: el archivo tenía tipo tmp_t (se movió con mv)
# audit2allow genera: allow httpd_t tmp_t:file { read };
# Ahora httpd puede leer TODOS los archivos tmp_t ← peligroso
# La solución correcta era: restorecon -v archivo

# BIEN — diagnosticar primero
ausearch -m AVC -ts today | audit2why
# 1. ¿Boolean? → setsebool
# 2. ¿File context? → restorecon o semanage fcontext
# 3. ¿Puerto? → semanage port
# 4. Solo si nada de lo anterior → audit2allow
```

### Error 2: No reproducir el problema antes de generar política

```bash
# MAL — generar política de AVC acumulados de semanas
ausearch -m AVC | audit2allow -M big_policy
# Incluye AVC de muchos eventos diferentes, algunos pueden ser
# intentos de ataque reales que NO deberían permitirse

# BIEN — reproducir el caso específico
# 1. Limpiar el estado
ausearch -m AVC -ts recent    # verificar que está limpio
# 2. Reproducir solo la operación que falla
systemctl restart httpd       # o la acción que causa el error
# 3. Capturar solo los AVC nuevos
ausearch -m AVC -ts recent | audit2allow -M httpd_fix
```

### Error 3: No revisar el .te antes de instalar

```bash
# SIEMPRE leer el archivo .te generado
cat my_policy.te

# Señales de alarma:
# - Tipos genéricos como file_type, domain, etc.
# - Muchos permisos { read write execute open ... }
# - Reglas con unconfined_t como fuente o destino
# - Comentarios "permissive dominio_t" (desconfina el proceso entero)

# Si ves algo sospechoso → NO instalar
# Buscar la solución correcta (file context, boolean, port)
```

### Error 4: Olvidar semodule -B después de semodule -DB

```bash
# semodule -DB desactiva TODAS las reglas dontaudit
# Esto genera miles de AVC por minuto en el log

# Si lo olvidas:
# /var/log/audit/audit.log crece rápidamente
# El disco puede llenarse
# El rendimiento del sistema puede degradarse

# Verificar si dontaudit está desactivado:
# (No hay comando directo, pero si el log tiene AVC inusuales
#  como getattr en /proc/*, probablemente dontaudit está off)

# Restaurar siempre:
semodule -B
```

### Error 5: Confundir la fuente (scontext) con el objetivo (tcontext)

```
type=AVC ... denied { read }
  scontext=system_u:system_r:httpd_t:s0
  tcontext=system_u:object_r:user_home_t:s0

scontext = SOURCE context = el PROCESO que intenta la operación
           httpd_t → Apache
tcontext = TARGET context = el OBJETO al que intenta acceder
           user_home_t → un archivo en /home

Confundir los dos lleva a diagnósticos incorrectos:
  MAL: "el archivo httpd_t no puede..." → httpd_t es el proceso
  BIEN: "Apache (httpd_t) no puede leer un archivo user_home_t"
```

---

## 10. Cheatsheet

```
=== BUSCAR AVC DENIALS ===
ausearch -m AVC -ts recent            últimos 10 minutos
ausearch -m AVC -ts today             hoy
ausearch -m AVC -c httpd              solo de httpd
ausearch -m AVC -ts recent | audit2why    buscar + explicar
ausearch -m AVC -ts recent -i         formato legible

=== ENTENDER LA CAUSA ===
audit2why                             explica POR QUÉ se denegó
  "Boolean XXX"                       → setsebool -P XXX on
  "Missing TE rule"                   → file context, port, o política

sealert -a /var/log/audit/audit.log   diagnóstico amigable con sugerencias
sealert -l UUID                       diagnóstico de una alerta específica

=== LEER UN AVC ===
{ operación }                         qué se intentó (read, write, name_bind)
comm="proceso"                        quién lo intentó
scontext=...:tipo_t:...               dominio del proceso (source)
tcontext=...:tipo_t:...               tipo del objetivo (target)
tclass=file|dir|tcp_socket            clase del objeto

=== FLUJO DE RESOLUCIÓN ===
1. ausearch + audit2why               ¿qué pasa?
2. ¿Boolean?                          setsebool -P
3. ¿File context incorrecto?          restorecon o semanage fcontext
4. ¿Puerto sin tipo?                  semanage port -a
5. ¿Nada funciona?                    audit2allow -M (último recurso)

=== audit2allow ===
ausearch ... | audit2allow            ver regla propuesta (sin aplicar)
ausearch ... | audit2allow -M nombre  generar módulo (.te + .pp)
cat nombre.te                         REVISAR antes de instalar
semodule -i nombre.pp                 instalar módulo
semodule -r nombre                    eliminar módulo

=== dontaudit ===
semodule -DB                          desactivar dontaudit (debug)
semodule -B                           restaurar dontaudit (SIEMPRE)

=== HERRAMIENTAS ===
ausearch         buscar en audit.log (paquete: audit)
audit2why        explicar AVCs (paquete: policycoreutils-python-utils)
audit2allow      generar política (paquete: policycoreutils-python-utils)
sealert          diagnóstico amigable (paquete: setroubleshoot-server)
```

---

## 11. Ejercicios

### Ejercicio 1: Interpretar mensajes AVC

Analiza estos mensajes AVC y para cada uno indica: qué proceso intentó qué operación sobre qué objeto, por qué fue denegado, y cuál es la solución:

**AVC 1:**
```
type=AVC msg=audit(1711008000.100:100): avc: denied { name_connect } for pid=2345 comm="httpd" dest=5432 scontext=system_u:system_r:httpd_t:s0 tcontext=system_u:object_r:postgresql_port_t:s0 tclass=tcp_socket permissive=0
```

**AVC 2:**
```
type=AVC msg=audit(1711008000.200:200): avc: denied { write } for pid=2345 comm="httpd" name="cache" dev="sda1" ino=67890 scontext=system_u:system_r:httpd_t:s0 tcontext=system_u:object_r:httpd_sys_content_t:s0 tclass=dir permissive=0
```

**AVC 3:**
```
type=AVC msg=audit(1711008000.300:300): avc: denied { read } for pid=2345 comm="httpd" name="app.conf" dev="sda1" ino=11111 scontext=system_u:system_r:httpd_t:s0 tcontext=unconfined_u:object_r:tmp_t:s0 tclass=file permissive=0
```

> **Pregunta de predicción**: Para el AVC 2, el directorio `cache` tiene tipo `httpd_sys_content_t` (solo lectura). ¿Qué tipo debería tener para que Apache pueda escribir? ¿Usarías un boolean o un cambio de file context?

### Ejercicio 2: Troubleshooting completo

Un servidor RHEL ejecuta una aplicación web con estos componentes:
- Nginx como reverse proxy (puerto 443)
- Una API Python en Gunicorn (puerto 8000)
- PostgreSQL (puerto 5432)

Después de activar SELinux enforcing, reportan estos problemas:

1. Nginx no puede conectar al backend Gunicorn en puerto 8000
2. Los archivos estáticos en `/opt/webapp/static/` dan 403
3. La API no puede conectar a PostgreSQL

Para cada problema:
1. ¿Qué AVC esperarías ver?
2. Escribe el comando `ausearch` para encontrarlo
3. ¿Qué diría `audit2why`?
4. ¿Cuál es la solución? (boolean, file context, port, o política)

> **Pregunta de predicción**: Si Gunicorn corre como `unconfined_service_t` (sin política SELinux propia), ¿tendrá el mismo problema de conexión a PostgreSQL que tendría si fuera `httpd_t`? ¿Por qué?

### Ejercicio 3: Evaluar audit2allow

Un administrador generó esta política con audit2allow:

```
module my_webapp 1.0;

require {
    type httpd_t;
    type tmp_t;
    type default_t;
    type var_t;
    type user_home_t;
    class file { read write open getattr execute };
    class dir { search read open getattr };
}

#============= httpd_t ==============
allow httpd_t tmp_t:file { read write open getattr execute };
allow httpd_t default_t:file { read open getattr };
allow httpd_t var_t:file { read write open getattr };
allow httpd_t user_home_t:file { read open getattr };
allow httpd_t user_home_t:dir { search read open getattr };
```

1. ¿Cuántas de estas reglas son probablemente innecesarias? ¿Cuáles?
2. La regla `allow httpd_t tmp_t:file { ... execute }` incluye `execute`. ¿Por qué es peligroso?
3. ¿Qué tipos en esta política son "genéricos" que sugieren que el archivo tiene tipo incorrecto?
4. Si arreglas los file contexts correctamente, ¿cuántas de estas reglas seguirían siendo necesarias?
5. Reescribe el procedimiento que el administrador debería haber seguido

> **Pregunta de predicción**: Si el administrador instala esta política y un atacante consigue escribir un script malicioso en `/tmp/`, ¿Apache podrá ejecutarlo? ¿Qué regla específica lo permitiría?

---

> **Siguiente tema**: T04 — Ports (semanage port, permitir servicios en puertos no estándar).
