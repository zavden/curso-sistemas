# File contexts — semanage fcontext, restorecon, chcon

## Índice

1. [¿Qué son los file contexts?](#1-qué-son-los-file-contexts)
2. [La base de datos de file contexts](#2-la-base-de-datos-de-file-contexts)
3. [restorecon — restaurar contextos correctos](#3-restorecon--restaurar-contextos-correctos)
4. [chcon — cambio temporal de contexto](#4-chcon--cambio-temporal-de-contexto)
5. [semanage fcontext — cambio permanente de contexto](#5-semanage-fcontext--cambio-permanente-de-contexto)
6. [chcon vs semanage fcontext: temporal vs permanente](#6-chcon-vs-semanage-fcontext-temporal-vs-permanente)
7. [Escenarios prácticos](#7-escenarios-prácticos)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. ¿Qué son los file contexts?

Cada archivo en un sistema con SELinux tiene un contexto de seguridad almacenado como **atributo extendido** (xattr) del sistema de archivos. El tipo del archivo determina qué procesos pueden acceder a él.

```
/var/www/html/index.html
  ├── Permisos DAC: -rw-r--r-- apache apache
  └── Contexto SELinux: system_u:object_r:httpd_sys_content_t:s0
                                          ^^^^^^^^^^^^^^^^^^^^^
                                          El tipo determina quién
                                          puede leer este archivo

httpd_t puede leer httpd_sys_content_t → ✓ Apache puede servir este archivo
sshd_t  no puede leer httpd_sys_content_t → ✗ SSH no puede acceder
```

### Dónde se almacena el contexto

```bash
# El contexto está en los extended attributes del archivo
getfattr -n security.selinux /var/www/html/index.html
# security.selinux="system_u:object_r:httpd_sys_content_t:s0"

# Sistemas de archivos que soportan xattr SELinux:
# ext4, xfs, btrfs → soportan xattrs ✓
# vfat, ntfs       → NO soportan xattrs (se monta con context=)
# NFS              → depende de la versión y config
# tmpfs            → soporta xattrs ✓
```

---

## 2. La base de datos de file contexts

SELinux mantiene una base de datos que mapea **rutas** a **contextos esperados**. Esta base de datos define qué contexto DEBERÍA tener cada archivo según su ubicación.

### Consultar la base de datos

```bash
# Ver todas las reglas de contexto
semanage fcontext -l | head -20
# SELinux fcontext       type          Context
# /                      directory     system_u:object_r:root_t:s0
# /.*                    all files     system_u:object_r:default_t:s0
# /bin/.*                all files     system_u:object_r:bin_t:s0
# /etc/.*                all files     system_u:object_r:etc_t:s0
# /var/www(/.*)?         all files     system_u:object_r:httpd_sys_content_t:s0
# /var/log(/.*)?         all files     system_u:object_r:var_log_t:s0

# Buscar contexto de una ruta específica
semanage fcontext -l | grep '/var/www'
# /var/www(/.*)?                    all files  system_u:object_r:httpd_sys_content_t:s0
# /var/www/html(/.*)?/wp-content(/.*)?  all files  ...httpd_sys_rw_content_t:s0
# ...

# Ver qué contexto DEBERÍA tener un archivo
matchpathcon /var/www/html/index.html
# /var/www/html/index.html    system_u:object_r:httpd_sys_content_t:s0

matchpathcon /etc/shadow
# /etc/shadow    system_u:object_r:shadow_t:s0

matchpathcon /srv/myapp/data
# /srv/myapp/data    system_u:object_r:var_t:s0
# ^^^ no hay regla específica para /srv/myapp → usa la genérica de /srv
```

### Estructura de la base de datos

```
Las reglas file_contexts vienen de dos fuentes:

1. Política base (empaquetada en selinux-policy-targeted)
   → /etc/selinux/targeted/contexts/files/file_contexts
   → NO editar directamente

2. Reglas locales (añadidas por el admin con semanage fcontext)
   → /etc/selinux/targeted/contexts/files/file_contexts.local
   → Gestionadas con semanage fcontext -a/-d/-m

semanage fcontext -l muestra AMBAS combinadas
semanage fcontext -l -C muestra SOLO las locales (personalizadas)
```

### Formato de las reglas

```bash
# Las reglas usan expresiones regulares de la ruta
# Formato: REGEX    FILE_TYPE    CONTEXT

/var/www(/.*)?          all files    httpd_sys_content_t
# ^^^^^^^^^^^
# Regex: /var/www seguido de cualquier cosa (incluyendo nada)
# Aplica a: /var/www, /var/www/html, /var/www/html/index.html, etc.

/var/log/httpd(/.*)?    all files    httpd_log_t
# Aplica a todo dentro de /var/log/httpd/

/etc/httpd(/.*)?        all files    httpd_config_t
# Aplica a la configuración de Apache
```

---

## 3. restorecon — restaurar contextos correctos

`restorecon` lee la base de datos de file contexts y **aplica el contexto correcto** a los archivos según su ruta. Es la herramienta más importante para arreglar contextos.

### Uso básico

```bash
# Restaurar contexto de un archivo
restorecon -v /var/www/html/index.html
# Relabeled /var/www/html/index.html from
#   unconfined_u:object_r:tmp_t:s0 to
#   system_u:object_r:httpd_sys_content_t:s0

# -v: verbose (muestra qué cambió)
# Sin -v: silencioso (no muestra nada si no hay cambios)
```

### Recursivo

```bash
# Restaurar todo un directorio y su contenido
restorecon -Rv /var/www/html/
# Relabeled /var/www/html/page.html from ...tmp_t to ...httpd_sys_content_t
# Relabeled /var/www/html/style.css from ...tmp_t to ...httpd_sys_content_t
# Relabeled /var/www/html/uploads from ...tmp_t to ...httpd_sys_rw_content_t

# -R: recursivo
# -v: verbose
```

### Dry run (ver sin cambiar)

```bash
# Ver qué cambiaría sin aplicar
restorecon -Rvn /var/www/html/
# -n: no changes, solo muestra
# Útil para verificar antes de aplicar en producción
```

### Forzar relabel

```bash
# Forzar incluso si el contexto parece correcto
restorecon -RvF /var/www/html/
# -F: fuerza el re-etiquetado del campo user y role también
#     (no solo el tipo)
```

### Cuándo usar restorecon

```
1. Después de mv (mover archivos)
   mv /tmp/file /var/www/html/
   restorecon -v /var/www/html/file

2. Después de extraer un tar/zip
   tar xzf site.tar.gz -C /var/www/html/
   restorecon -Rv /var/www/html/

3. Después de cambiar file contexts con semanage fcontext
   semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
   restorecon -Rv /srv/web/    ← OBLIGATORIO para aplicar

4. Cuando un servicio falla con AVC denial de tipo incorrecto
   ausearch -m AVC -ts recent  → archivo tiene tipo default_t o tmp_t
   restorecon -Rv /ruta/del/archivo

5. Después de reactivar SELinux desde disabled
   touch /.autorelabel && reboot
   (o fixfiles relabel)
```

---

## 4. chcon — cambio temporal de contexto

`chcon` cambia el contexto SELinux de un archivo **directamente** en el sistema de archivos. El cambio NO se registra en la base de datos de file contexts.

### Uso básico

```bash
# Cambiar el tipo de un archivo
chcon -t httpd_sys_content_t /srv/web/index.html

# Cambiar el tipo recursivamente
chcon -R -t httpd_sys_content_t /srv/web/

# Cambiar el contexto completo
chcon system_u:object_r:httpd_sys_content_t:s0 /srv/web/index.html

# Copiar el contexto de un archivo a otro
chcon --reference=/var/www/html/index.html /srv/web/index.html
# El archivo destino recibe el mismo contexto que el de referencia
```

### Por qué es temporal

```
chcon cambia el xattr del archivo directamente
PERO no modifica la base de datos de file contexts

Consecuencia:
  chcon -t httpd_sys_content_t /srv/web/index.html
  ls -Z /srv/web/index.html
  → httpd_sys_content_t ✓ (funciona ahora)

  restorecon -v /srv/web/index.html
  → Relabeled from httpd_sys_content_t to var_t
  → ¡Volvió al tipo de la base de datos! ✗

  También se pierde con:
  → fixfiles relabel (relabel completo)
  → touch /.autorelabel && reboot
```

```
chcon:              ARCHIVO ← nuevo contexto (xattr directo)
                    Base de datos: sin cambios
                    restorecon: SOBRESCRIBE el cambio de chcon

semanage fcontext:  Base de datos ← nueva regla
                    ARCHIVO: sin cambios todavía
                    restorecon: APLICA la regla al archivo
```

### Cuándo usar chcon

```
SOLO para pruebas temporales rápidas:

  1. Sospechas que un tipo incorrecto causa un problema
  2. chcon -t tipo_correcto archivo
  3. ¿Funciona ahora? → Sí → el tipo era el problema
  4. Ahora hacer el cambio permanente con semanage fcontext

  Es el equivalente de setenforce 0 pero para UN archivo:
  prueba rápida, no solución permanente
```

---

## 5. semanage fcontext — cambio permanente de contexto

`semanage fcontext` modifica la **base de datos** de file contexts. Los cambios persisten y sobreviven a `restorecon` y relabels.

### Añadir una regla nueva

```bash
# Sintaxis:
semanage fcontext -a -t TIPO "REGEX_RUTA"

# Ejemplo: todo en /srv/web debe tener tipo httpd_sys_content_t
semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
#                  ^   ^                    ^^^^^^^^^^^^^^^^^^^^
#                  │   │                    regex de la ruta
#                  │   └── tipo a asignar
#                  └── add (añadir regla)

# IMPORTANTE: esto solo modifica la base de datos
# Los archivos en disco NO cambian todavía
# Necesitas aplicar con restorecon:
restorecon -Rv /srv/web/
```

### El flujo completo (los dos pasos)

```
Paso 1: Definir la regla (semanage fcontext)
  → Modifica /etc/selinux/targeted/contexts/files/file_contexts.local
  → Los archivos en disco NO cambian

Paso 2: Aplicar la regla (restorecon)
  → Lee la base de datos
  → Cambia los xattrs de los archivos en disco
  → Los archivos ahora tienen el contexto correcto

AMBOS pasos son necesarios. Olvidar cualquiera = no funciona.
```

```bash
# Ejemplo completo: servir contenido web desde /srv/webapp

# 1. Crear la regla de contexto
semanage fcontext -a -t httpd_sys_content_t "/srv/webapp(/.*)?"

# 2. Crear una regla para el directorio de uploads (lectura+escritura)
semanage fcontext -a -t httpd_sys_rw_content_t "/srv/webapp/uploads(/.*)?"

# 3. Aplicar a los archivos existentes
restorecon -Rv /srv/webapp/

# 4. Verificar
ls -Z /srv/webapp/
# ...httpd_sys_content_t...    index.html
# ...httpd_sys_content_t...    style.css
# ...httpd_sys_rw_content_t... uploads/
```

### Modificar una regla existente

```bash
# Cambiar el tipo de una regla existente
semanage fcontext -m -t httpd_sys_rw_content_t "/srv/webapp(/.*)?"
#                  ^
#                  modify (modificar regla existente)

restorecon -Rv /srv/webapp/
```

### Eliminar una regla local

```bash
# Eliminar una regla que tú añadiste
semanage fcontext -d "/srv/webapp(/.*)?"
#                  ^
#                  delete

# Solo puede eliminar reglas LOCALES (las que añadiste con -a)
# No puede eliminar reglas de la política base
```

### Ver reglas locales (personalizadas)

```bash
# Solo reglas añadidas por el admin
semanage fcontext -l -C
# /srv/webapp(/.*)?     all files    system_u:object_r:httpd_sys_content_t:s0
# /srv/webapp/uploads(/.*)?  all files  system_u:object_r:httpd_sys_rw_content_t:s0

# Esto es lo que necesitas replicar en otro servidor
```

### Regex en las rutas

```bash
# Directorio y todo su contenido (el patrón más común)
"/srv/webapp(/.*)?"
# /srv/webapp             ← el directorio
# /srv/webapp/            ← con barra
# /srv/webapp/file.html   ← archivos directos
# /srv/webapp/sub/dir/f   ← subdirectorios anidados

# Solo archivos directos (no subdirectorios)
"/srv/webapp/[^/]*"
# /srv/webapp/file.html   ← sí
# /srv/webapp/sub/file    ← no

# Un archivo específico
"/srv/webapp/config\.php"
# Solo /srv/webapp/config.php

# Subdirectorio específico y su contenido
"/srv/webapp/uploads(/.*)?"
# /srv/webapp/uploads/
# /srv/webapp/uploads/photo.jpg
# /srv/webapp/uploads/2026/photo.jpg
```

---

## 6. chcon vs semanage fcontext: temporal vs permanente

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  chcon                          semanage fcontext            │
│  ─────                          ──────────────────           │
│  Cambia el archivo              Cambia la base de datos      │
│  directamente (xattr)           (regla de contexto)          │
│                                                              │
│  NO modifica la base            NO modifica el archivo       │
│  de datos                       directamente                 │
│                                                              │
│  restorecon SOBRESCRIBE         restorecon APLICA             │
│  el cambio de chcon             la regla de semanage         │
│                                                              │
│  TEMPORAL                       PERMANENTE                   │
│  (pruebas rápidas)              (producción)                 │
│                                                              │
│  1 paso:                        2 pasos:                     │
│    chcon -t tipo archivo          semanage fcontext -a ...   │
│                                   restorecon -Rv ...         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Demostración

```bash
# === chcon (temporal) ===
ls -Z /srv/data/file.txt
# ...var_t:s0 file.txt

chcon -t httpd_sys_content_t /srv/data/file.txt
ls -Z /srv/data/file.txt
# ...httpd_sys_content_t:s0 file.txt    ← cambió ✓

restorecon -v /srv/data/file.txt
# Relabeled from httpd_sys_content_t to var_t
ls -Z /srv/data/file.txt
# ...var_t:s0 file.txt                  ← volvió al original ✗

# === semanage fcontext (permanente) ===
semanage fcontext -a -t httpd_sys_content_t "/srv/data(/.*)?"
restorecon -Rv /srv/data/
# Relabeled from var_t to httpd_sys_content_t
ls -Z /srv/data/file.txt
# ...httpd_sys_content_t:s0 file.txt    ← cambió ✓

restorecon -v /srv/data/file.txt
# (sin cambios — ya tiene el tipo correcto)
# Sobrevive a restorecon, relabel, reboot ✓
```

### Cuándo usar cada uno

| Situación | Herramienta | Razón |
|---|---|---|
| Probar si un tipo resuelve un problema | `chcon` | Rápido, reversible con restorecon |
| Ubicación personalizada de contenido web | `semanage fcontext` + `restorecon` | Permanente, sobrevive a relabel |
| Arreglar un archivo que fue movido con `mv` | `restorecon` | Ya tiene la regla correcta en la BD |
| Cambiar el tipo de un directorio no estándar | `semanage fcontext` + `restorecon` | Permanente |
| Producción | **Siempre semanage** | Nunca chcon en producción |

---

## 7. Escenarios prácticos

### Escenario 1: Servir web desde ubicación no estándar

```bash
# Apache debe servir desde /data/websites/empresa/
# DocumentRoot /data/websites/empresa en httpd.conf

# Problema: los archivos tienen tipo default_t
ls -Z /data/websites/empresa/
# ...default_t... index.html    ← Apache no puede leer

# Solución:
# 1. Definir el contexto
semanage fcontext -a -t httpd_sys_content_t "/data/websites(/.*)?"

# 2. Si hay un directorio de uploads que Apache necesita escribir:
semanage fcontext -a -t httpd_sys_rw_content_t "/data/websites/empresa/uploads(/.*)?"

# 3. Aplicar
restorecon -Rv /data/websites/

# 4. Verificar
ls -Z /data/websites/empresa/
# ...httpd_sys_content_t...    index.html ✓
# ...httpd_sys_rw_content_t... uploads/ ✓
```

### Escenario 2: MySQL con datos en ubicación personalizada

```bash
# MySQL movido a /data/mysql/ (disco dedicado)
# datadir=/data/mysql en my.cnf

# Problema: archivos tienen tipo default_t
ls -Z /data/mysql/
# ...default_t... ibdata1    ← MySQL no puede leer

# Solución:
semanage fcontext -a -t mysqld_db_t "/data/mysql(/.*)?"
restorecon -Rv /data/mysql/

ls -Z /data/mysql/
# ...mysqld_db_t... ibdata1 ✓
# ...mysqld_db_t... ib_logfile0 ✓
# ...mysqld_db_t... mydb/ ✓
```

### Escenario 3: Archivos movidos con mv

```bash
# Admin mueve la web nueva
mv /home/admin/new-site/* /var/www/html/

# Problema: archivos conservan tipo de /home
ls -Z /var/www/html/
# ...user_home_t... index.html    ← tipo incorrecto

# Solución: restorecon (la regla ya existe para /var/www)
restorecon -Rv /var/www/html/
# Relabeled from user_home_t to httpd_sys_content_t ✓

# No se necesita semanage fcontext porque /var/www/html ya tiene
# regla en la política base
```

### Escenario 4: Home directory compartido vía Apache

```bash
# Apache sirve archivos del home de un usuario
# Alias /alice /home/alice/public_html

# Opción 1: Boolean (si solo necesitas leer homes)
setsebool -P httpd_enable_homedirs on

# Opción 2: Cambiar contexto del directorio específico
semanage fcontext -a -t httpd_sys_content_t "/home/alice/public_html(/.*)?"
restorecon -Rv /home/alice/public_html/

# Opción 2 es más restrictiva (mínimo privilegio):
# Solo /home/alice/public_html es accesible, no todos los homes
```

### Escenario 5: Contenido web en sistema de archivos sin xattr

```bash
# Mount VFAT (USB) o NFS sin soporte SELinux
# Los archivos no tienen xattrs → no tienen contexto

# Solución: montar con contexto explícito
mount -t vfat /dev/sdb1 /mnt/usb -o context=system_u:object_r:httpd_sys_content_t:s0
# Todos los archivos del mount obtienen este contexto

# En /etc/fstab:
/dev/sdb1  /mnt/usb  vfat  context=system_u:object_r:httpd_sys_content_t:s0  0 0

# Para NFS:
mount -t nfs server:/share /mnt/nfs -o context=system_u:object_r:httpd_sys_content_t:s0
# O usar el boolean httpd_use_nfs
```

---

## 8. Errores comunes

### Error 1: Usar semanage fcontext sin restorecon

```bash
# MAL — solo definir la regla
semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
# "Ya lo configuré pero Apache sigue dando 403"

ls -Z /srv/web/
# ...default_t...    ← no cambió

# La base de datos tiene la regla, pero los archivos no se actualizaron

# BIEN — siempre los dos pasos
semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
restorecon -Rv /srv/web/    ← OBLIGATORIO
```

### Error 2: Usar chcon en producción

```bash
# MAL — parece que funciona pero es temporal
chcon -R -t httpd_sys_content_t /srv/web/
# Funciona... hasta que alguien ejecuta restorecon o un relabel

# 6 meses después: actualización de selinux-policy → relabel automático
# Todo vuelve a default_t → Apache falla → "¿qué cambió?"

# BIEN — siempre semanage fcontext para cambios permanentes
semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
restorecon -Rv /srv/web/
```

### Error 3: Regex incorrecta en semanage fcontext

```bash
# MAL — sin el patrón de subdirectorios
semanage fcontext -a -t httpd_sys_content_t "/srv/web"
# Solo aplica a /srv/web exactamente, no a su contenido

restorecon -Rv /srv/web/
# Solo /srv/web cambia, los archivos dentro NO

# BIEN — incluir el patrón recursivo
semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
# /srv/web + todo lo que contenga
```

```bash
# Patrones correctos:
"/srv/web(/.*)?"           ← directorio + todo su contenido ✓
"/srv/web(/.*)"            ← solo contenido, no el directorio ✗ (casi siempre error)
"/srv/web"                 ← solo el directorio, no contenido ✗
"/srv/web/*"               ← NO es glob, es regex → no funciona como esperas ✗
```

### Error 4: Confundir por qué un archivo tiene tipo incorrecto

```bash
# Archivo con tipo incorrecto. ¿Cuál es la causa?

ls -Z /var/www/html/new.html
# ...tmp_t...

# Posibles causas:
# 1. Se movió con mv desde /tmp → restorecon lo arregla
# 2. Se creó mientras SELinux estaba disabled → restorecon lo arregla
# 3. No hay regla de file_contexts para esta ruta → necesita semanage fcontext

# Diagnóstico:
matchpathcon /var/www/html/new.html
# /var/www/html/new.html    httpd_sys_content_t
# ^^^ Si matchpathcon devuelve el tipo correcto → causa 1 o 2 → restorecon

matchpathcon /srv/custom/data.txt
# /srv/custom/data.txt    var_t
# ^^^ Si devuelve un tipo genérico → causa 3 → necesita semanage fcontext
```

### Error 5: No entender la prioridad de reglas

```bash
# ¿Qué pasa si dos reglas coinciden?
semanage fcontext -l | grep '/var/www'
# /var/www(/.*)?                        httpd_sys_content_t
# /var/www/html(/.*)?/wp-content(/.*)?  httpd_sys_rw_content_t

# Regla: la más específica gana (la regex más larga que coincide)
# /var/www/html/wp-content/uploads/photo.jpg
#   → coincide con /var/www(/.*)?           → httpd_sys_content_t
#   → coincide con /var/www/html/.../wp-content(/.*)? → httpd_sys_rw_content_t
#   → gana la segunda (más específica) ✓
```

---

## 9. Cheatsheet

```
=== CONSULTAR CONTEXTOS ===
ls -Z archivo                       ver contexto actual del archivo
ls -Zd directorio                   ver contexto del directorio
matchpathcon /ruta                  qué contexto DEBERÍA tener
semanage fcontext -l | grep RUTA    buscar reglas de la base de datos
semanage fcontext -l -C             solo reglas LOCALES (personalizadas)

=== restorecon (RESTAURAR desde base de datos) ===
restorecon -v archivo               restaurar un archivo (verbose)
restorecon -Rv directorio/          restaurar recursivamente
restorecon -Rvn directorio/         dry run (ver sin cambiar)
restorecon -RvF directorio/         forzar (incluye user/role)

Cuándo usar:
  → Después de mv (mover archivos)
  → Después de tar/unzip
  → Después de semanage fcontext -a
  → Cuando un archivo tiene tipo incorrecto

=== chcon (CAMBIO TEMPORAL) ===
chcon -t TIPO archivo               cambiar tipo
chcon -R -t TIPO directorio/        cambiar recursivamente
chcon --reference=origen destino     copiar contexto de otro archivo

TEMPORAL: restorecon SOBRESCRIBE los cambios de chcon
Usar solo para PRUEBAS rápidas, nunca en producción

=== semanage fcontext (CAMBIO PERMANENTE) ===
semanage fcontext -a -t TIPO "REGEX"    añadir regla
semanage fcontext -m -t TIPO "REGEX"    modificar regla existente
semanage fcontext -d "REGEX"            eliminar regla local

SIEMPRE seguido de: restorecon -Rv /ruta/

Regex comunes:
  "/srv/web(/.*)?"        directorio + todo su contenido
  "/srv/web/[^/]*"        solo archivos directos
  "/srv/web/config\.php"  un archivo específico

=== TIPOS COMUNES PARA WEB ===
httpd_sys_content_t       lectura (HTML, CSS, JS, imágenes)
httpd_sys_rw_content_t    lectura + escritura (uploads, cache)
httpd_sys_script_exec_t   scripts CGI ejecutables
httpd_log_t               logs de Apache
httpd_config_t            configuración de Apache

=== FLUJO DE TRABAJO ===
Prueba rápida:
  chcon -t tipo archivo → ¿funciona? → sí → hacer permanente

Cambio permanente:
  semanage fcontext -a -t tipo "regex"
  restorecon -Rv /ruta/

Archivo movido con mv:
  restorecon -v archivo    (la regla ya existe)

=== DIAGNÓSTICO ===
1. ls -Z archivo              ¿qué tipo tiene?
2. matchpathcon /ruta          ¿qué tipo debería tener?
3. Si difieren → restorecon    (si la regla existe)
                → semanage fcontext  (si necesitas regla nueva)
```

---

## 10. Ejercicios

### Ejercicio 1: Arreglar contextos incorrectos

Un servidor web tiene estos archivos con contextos SELinux:

```bash
$ ls -Z /var/www/html/
unconfined_u:object_r:user_home_t:s0     about.html
system_u:object_r:httpd_sys_content_t:s0 index.html
unconfined_u:object_r:tmp_t:s0           contact.html
system_u:object_r:httpd_sys_content_t:s0 style.css
unconfined_u:object_r:default_t:s0       products/
```

1. ¿Cuáles de estos archivos puede leer Apache (`httpd_t`)? ¿Cuáles no?
2. ¿Qué causó que `about.html` tenga tipo `user_home_t`? ¿Y `contact.html` con `tmp_t`?
3. ¿Qué comando arregla TODOS los contextos de una vez?
4. ¿Necesitas usar `semanage fcontext` o basta con `restorecon`? ¿Por qué?
5. Después de arreglarlo, ¿qué tipo tendrán todos los archivos?

> **Pregunta de predicción**: Si ejecutas `restorecon -Rv /var/www/html/products/` pero `products/` se creó mientras SELinux estaba disabled, ¿restorecon asignará el tipo correcto? ¿O necesitas algo adicional?

### Ejercicio 2: Configurar una ubicación personalizada

Tu empresa tiene esta estructura en disco:

```
/data/apps/
├── frontend/           ← contenido web estático (solo lectura para Apache)
│   ├── index.html
│   ├── css/
│   └── js/
├── backend/            ← API Node.js (no accedida por Apache)
│   └── server.js
├── uploads/            ← usuarios suben archivos vía la web (lectura+escritura)
│   └── photos/
└── logs/               ← logs de Apache
    ├── access.log
    └── error.log
```

Escribe los comandos necesarios para:

1. Que Apache pueda leer todo en `frontend/`
2. Que Apache pueda leer Y escribir en `uploads/`
3. Que Apache pueda escribir logs en `logs/`
4. Que `backend/` NO sea accesible por Apache (debe mantener un tipo que Apache no pueda leer)
5. Verificar que todo está correcto

Para cada paso, incluye tanto el `semanage fcontext` como el `restorecon`.

> **Pregunta de predicción**: Si más adelante añades un directorio `/data/apps/frontend/images/`, ¿necesitas ejecutar `semanage fcontext` otra vez o basta con `restorecon`? ¿Por qué?

### Ejercicio 3: Diagnosticar y elegir la herramienta correcta

Para cada situación, indica si usarías `chcon`, `restorecon`, `semanage fcontext` + `restorecon`, o ninguno. Justifica:

1. Descomprimiste un tar de backup en `/var/www/html/` y Apache da 403
2. Quieres probar si el tipo `httpd_sys_rw_content_t` resuelve un problema en `/var/www/html/cache/`
3. Montaste un disco USB en `/mnt/usb` y quieres que Apache lea su contenido
4. Creaste un directorio nuevo `/opt/webapp/static/` para servir con Apache
5. Un compañero usó `chcon` hace 3 meses para arreglar `/srv/data/` y ahora tras una actualización de SELinux todo falla de nuevo

Para la situación 5, explica por qué falló y cuál es la solución correcta.

> **Pregunta de predicción**: Si ejecutas `semanage fcontext -a -t httpd_sys_content_t "/opt/webapp(/.*)?"` y luego `semanage fcontext -a -t httpd_sys_rw_content_t "/opt/webapp/uploads(/.*)?"`, ¿la segunda regla sobrescribe a la primera para los archivos en `uploads/`? ¿O conviven? ¿Cuál gana para `/opt/webapp/uploads/photo.jpg`?

---

> **Siguiente tema**: T03 — Troubleshooting (audit2why, audit2allow, /var/log/audit/audit.log, sealert).
