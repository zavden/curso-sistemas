# Modos de SELinux — Enforcing, Permissive, Disabled

## Índice

1. [Los tres modos](#1-los-tres-modos)
2. [Consultar el modo actual](#2-consultar-el-modo-actual)
3. [Cambiar el modo en runtime](#3-cambiar-el-modo-en-runtime)
4. [Configuración persistente](#4-configuración-persistente)
5. [Modo Permissive por dominio](#5-modo-permissive-por-dominio)
6. [De Disabled a Enforcing: el re-etiquetado](#6-de-disabled-a-enforcing-el-re-etiquetado)
7. [Parámetros del kernel](#7-parámetros-del-kernel)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Los tres modos

SELinux opera en uno de tres modos que determinan cómo se aplica la política de seguridad:

```
┌─────────────────────────────────────────────────────────────────┐
│                                                                 │
│  ENFORCING          PERMISSIVE           DISABLED               │
│  ──────────         ──────────           ────────               │
│  Política activa    Política activa      Política NO cargada    │
│  Violaciones        Violaciones          Kernel sin SELinux      │
│  → DENEGADAS        → REGISTRADAS        Labels perdidos         │
│  + registradas      (no denegadas)       al crear archivos       │
│                                                                 │
│  Producción ✓       Debug/Testing ✓      NUNCA en producción    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Enforcing

```
Proceso httpd_t quiere leer shadow_t:

  Política: ¿httpd_t puede leer shadow_t? → NO
  Acción:   DENIEGA la operación
  Log:      type=AVC msg=audit(...): avc: denied { read } ...
  Resultado: httpd recibe "Permission denied"
```

- La política se **aplica**: las operaciones no permitidas son bloqueadas
- Las violaciones se registran en `/var/log/audit/audit.log`
- Es el modo normal de producción
- Es el modo por defecto en RHEL/Fedora

### Permissive

```
Proceso httpd_t quiere leer shadow_t:

  Política: ¿httpd_t puede leer shadow_t? → NO
  Acción:   PERMITE la operación (no bloquea)
  Log:      type=AVC msg=audit(...): avc: denied { read } ...
  Resultado: httpd LEE el archivo (como si SELinux no existiera)
```

- La política se **evalúa** pero no se aplica
- Las violaciones se registran igualmente (mismos logs que enforcing)
- Útil para **diagnosticar** qué está bloqueando SELinux
- Útil para **generar política** nueva (audit2allow)
- No es seguro para producción (no protege nada)

### Disabled

```
Proceso httpd_t quiere leer shadow_t:

  SELinux no está cargado en el kernel
  No hay evaluación de política
  No hay logs de AVC
  Los archivos nuevos NO reciben etiqueta SELinux
```

- SELinux está **completamente desactivado** en el kernel
- No se cargan módulos LSM de SELinux
- Los archivos creados mientras SELinux está disabled **no reciben contexto**
- Volver a enforcing requiere re-etiquetar todo el sistema de archivos
- **Nunca recomendado** — si necesitas desactivar, usa permissive

> **Disabled vs Permissive**: La diferencia clave es que en Permissive, SELinux sigue asignando etiquetas a archivos nuevos y sigue evaluando la política (solo que no la aplica). En Disabled, el kernel ni siquiera carga SELinux — los archivos nuevos pierden sus etiquetas, lo que causa problemas masivos al reactivar.

---

## 2. Consultar el modo actual

### getenforce

```bash
# Forma más rápida y simple
getenforce
# Enforcing     ← modo actual
# Permissive
# Disabled
```

### sestatus

```bash
# Información completa de SELinux
sestatus
# SELinux status:                 enabled
# SELinuxfs mount:                /sys/fs/selinux
# SELinux root directory:         /etc/selinux
# Loaded policy name:             targeted        ← tipo de política
# Current mode:                   enforcing       ← modo actual
# Mode from config file:          enforcing       ← modo persistente
# Policy MLS status:              enabled
# Policy deny_unknown status:     allowed
# Memory protection checking:     actual (secure)
# Max kernel policy version:      33
```

Los dos campos clave:

```
Current mode:          lo que está activo AHORA (puede haberse cambiado con setenforce)
Mode from config file: lo que se aplicará en el próximo REBOOT (/etc/selinux/config)

Si difieren: alguien cambió el modo en runtime sin hacerlo persistente
```

### Desde /proc

```bash
# El kernel expone el modo
cat /sys/fs/selinux/enforce
# 1 = enforcing
# 0 = permissive
# (si el directorio no existe → SELinux disabled o no compilado)
```

---

## 3. Cambiar el modo en runtime

### setenforce

```bash
# Cambiar a Permissive (temporalmente, no sobrevive reboot)
setenforce 0
# o
setenforce Permissive

# Cambiar a Enforcing
setenforce 1
# o
setenforce Enforcing

# Verificar
getenforce
```

**Restricciones de setenforce**:

```
setenforce puede:
  Enforcing  ↔  Permissive    ← cambiar entre estos dos

setenforce NO puede:
  Enforcing  →  Disabled      ← requiere reboot
  Permissive →  Disabled      ← requiere reboot
  Disabled   →  Enforcing     ← requiere reboot + relabel
  Disabled   →  Permissive    ← requiere reboot
```

```
┌────────────┐  setenforce 0  ┌────────────┐
│ Enforcing  │◄──────────────►│ Permissive │
│            │  setenforce 1  │            │
└────────────┘                └────────────┘
       ▲                            ▲
       │     Requiere reboot        │
       │     + /etc/selinux/config  │
       │                            │
       └──────────┬─────────────────┘
                  │
           ┌──────▼──────┐
           │  Disabled   │
           └─────────────┘
```

> `setenforce` requiere privilegios de root. En una política estricta, SELinux puede además impedir que ciertos dominios cambien el modo (por ejemplo, un proceso confinado no puede ejecutar `setenforce`).

### Cuándo usar setenforce 0

```
Escenario legítimo:
  1. Un servicio falla en producción
  2. Sospechas que SELinux lo bloquea
  3. setenforce 0  → ¿funciona ahora? → SÍ → era SELinux
  4. setenforce 1  → volver a enforcing
  5. Diagnosticar con audit2why y arreglar la política

Flujo CORRECTO:
  setenforce 0 → reproducir → confirmar → setenforce 1 → arreglar

Flujo INCORRECTO:
  setenforce 0 → "ya funciona" → olvidar → dejar en permissive para siempre
```

---

## 4. Configuración persistente

### /etc/selinux/config

```bash
# /etc/selinux/config
# (o symlink desde /etc/sysconfig/selinux)

# Modo de SELinux al arrancar
SELINUX=enforcing
# Valores posibles: enforcing, permissive, disabled

# Tipo de política
SELINUXTYPE=targeted
# Valores posibles: targeted, minimum, mls
```

```bash
# Cambiar el modo persistente
# Editar /etc/selinux/config y cambiar SELINUX=

# Ejemplo: pasar a permissive persistente
sed -i 's/^SELINUX=enforcing/SELINUX=permissive/' /etc/selinux/config
# Efecto: el modo cambia en el próximo reboot
# El modo ACTUAL no cambia hasta reiniciar
```

### Tipos de política

| Tipo | Descripción | Uso |
|---|---|---|
| `targeted` | Solo confina servicios de red y daemons específicos. El resto corre como `unconfined_t` (libre) | **Estándar en RHEL/Fedora** |
| `minimum` | Como targeted pero con menos módulos cargados | Entornos mínimos |
| `mls` | Multi-Level Security. Clasifica por niveles (top secret, secret, etc.) | Militar, gobierno |

La política `targeted` es la que usarás en el 99% de los casos. Confina ~200 servicios (httpd, sshd, named, mysqld, etc.) mientras deja a los usuarios normales sin confinar.

```
Política targeted:

  Confinados (tienen política específica):
    httpd_t, sshd_t, named_t, mysqld_t, postfix_t,
    dovecot_t, dhcpd_t, smbd_t, nfsd_t, ...

  No confinados (libres, como si SELinux no existiera):
    unconfined_t → usuarios de login, scripts manuales
    initrc_t → servicios sin política SELinux
```

### Verificar la política cargada

```bash
# Qué política está cargada
sestatus | grep "Loaded policy"
# Loaded policy name:             targeted

# Cuántos módulos de política están cargados
semodule -l | wc -l
# ~400 módulos en una instalación típica

# Buscar un módulo específico
semodule -l | grep httpd
# httpd
```

---

## 5. Modo Permissive por dominio

En vez de poner **todo** SELinux en permissive, puedes poner **un solo dominio** en permissive. Esto es mucho más seguro: el resto del sistema sigue protegido.

```bash
# Poner httpd_t en permissive (solo Apache deja de ser bloqueado)
semanage permissive -a httpd_t

# Verificar qué dominios están en permissive
semanage permissive -l
# Customized Permissive Types:
# httpd_t

# Quitar el permissive de un dominio
semanage permissive -d httpd_t
```

```
SELinux enforcing con httpd_t en permissive:

  sshd_t  → enforcing (protegido) ✓
  named_t → enforcing (protegido) ✓
  httpd_t → permissive (logs pero no bloquea) ⚠
  smbd_t  → enforcing (protegido) ✓

  Solo Apache opera sin restricciones.
  El resto del sistema sigue seguro.
```

### Cuándo usar permissive por dominio

```
Escenario: Estás migrando una aplicación web que genera
muchas violaciones SELinux. Necesitas generar política nueva.

MAL:
  setenforce 0  → TODO el sistema sin protección

BIEN:
  semanage permissive -a httpd_t
  → Solo Apache sin protección
  → Generar logs de AVC para httpd_t
  → audit2allow para crear política
  → Aplicar política
  → semanage permissive -d httpd_t  → Apache protegido de nuevo
```

---

## 6. De Disabled a Enforcing: el re-etiquetado

Si SELinux ha estado en `disabled`, los archivos creados durante ese período no tienen etiquetas SELinux. Volver a `enforcing` requiere re-etiquetar todo el sistema de archivos.

### El problema

```
Con SELinux disabled, se crean archivos:

  /var/www/html/new_page.html  → sin contexto SELinux
  /etc/httpd/conf.d/new.conf   → sin contexto SELinux
  /home/alice/notes.txt        → sin contexto SELinux

Si reactivamos SELinux sin re-etiquetar:
  SELinux ve archivos sin tipo → default_t o unlabeled_t
  httpd_t no puede leer default_t → TODO falla
```

### Procedimiento de re-etiquetado

```bash
# 1. Cambiar /etc/selinux/config
SELINUX=enforcing

# 2. Crear archivo de trigger para relabel al boot
touch /.autorelabel

# 3. Reiniciar
reboot

# El sistema al arrancar:
#   → Detecta /.autorelabel
#   → Ejecuta fixfiles -F relabel (re-etiqueta TODOS los archivos)
#   → Borra /.autorelabel
#   → Reinicia una segunda vez
#   → Arranca con SELinux enforcing y todos los archivos etiquetados

# ADVERTENCIA: el relabel puede tardar MUCHO en discos grandes
# Un disco de 500 GB puede tardar 10-30 minutos
# Un disco de varios TB puede tardar horas
```

### Re-etiquetar manualmente

```bash
# Re-etiquetar un directorio específico (rápido)
restorecon -Rv /var/www/html/
# Restaura las etiquetas según la política (file_contexts)

# Re-etiquetar todo el sistema (lento, pero sin reboot)
fixfiles -F relabel
# Recorre TODO el sistema de archivos

# Ver qué etiqueta se asignaría a un archivo
matchpathcon /var/www/html/index.html
# /var/www/html/index.html    system_u:object_r:httpd_sys_content_t:s0
```

### Por qué Disabled es peor que Permissive

```
Permissive:
  ├── SELinux cargado en el kernel ✓
  ├── Etiquetas se asignan a archivos nuevos ✓
  ├── Política se evalúa (logs) ✓
  ├── No bloquea ✗
  └── Volver a enforcing: setenforce 1 (inmediato) ✓

Disabled:
  ├── SELinux NO cargado en el kernel ✗
  ├── Archivos nuevos SIN etiqueta ✗
  ├── No hay logs de AVC ✗
  ├── No bloquea ✗
  └── Volver a enforcing: reboot + relabel (lento, riesgoso) ✗
```

---

## 7. Parámetros del kernel

El modo de SELinux se puede controlar desde la línea de comandos del kernel (GRUB), lo que es útil para recovery.

### Parámetros de boot

| Parámetro | Efecto |
|---|---|
| `enforcing=0` | Arranca en permissive (ignora /etc/selinux/config) |
| `enforcing=1` | Arranca en enforcing (ignora /etc/selinux/config) |
| `selinux=0` | Desactiva SELinux completamente en este boot |
| `selinux=1` | Activa SELinux (por defecto) |
| `autorelabel=1` | Fuerza re-etiquetado al arrancar |

### Uso en recovery

```
Escenario: SELinux enforcing + política rota = sistema no arranca

1. En GRUB, editar la entrada (presionar 'e')
2. Encontrar la línea que empieza con "linux" o "linuxefi"
3. Añadir al final: enforcing=0
4. Ctrl+X para arrancar

El sistema arranca en permissive → puedes entrar y arreglar

Alternativa más drástica:
  Añadir: selinux=0
  Arranca sin SELinux (como disabled pero solo para este boot)
```

```bash
# Verificar los parámetros del kernel actual
cat /proc/cmdline
# BOOT_IMAGE=... root=... enforcing=0 ...

# Verificar si SELinux fue habilitado vía kernel
cat /sys/fs/selinux/enforce
```

### GRUB2: deshabilitar selinux=0 en producción

En servidores de producción, puedes impedir que alguien use `selinux=0` en el boot:

```bash
# Proteger GRUB con contraseña (tema de C04)
# Si GRUB está protegido, no se pueden editar parámetros del kernel
# sin la contraseña de GRUB
```

---

## 8. Errores comunes

### Error 1: Cambiar el modo solo con setenforce y creer que es persistente

```bash
# MAL — solo runtime
setenforce 0
# Funciona AHORA, pero al reboot vuelve a enforcing
# (porque /etc/selinux/config sigue diciendo enforcing)

# BIEN — runtime + persistente
setenforce 0                    # efecto inmediato
# Editar /etc/selinux/config:
SELINUX=permissive              # efecto tras reboot

# O al revés, para volver a enforcing:
setenforce 1                    # efecto inmediato
# Verificar que /etc/selinux/config dice:
SELINUX=enforcing               # efecto tras reboot
```

### Error 2: Usar Disabled en vez de Permissive

```bash
# MAL — desactivar completamente
SELINUX=disabled
# Archivos nuevos pierden etiquetas
# Volver a enforcing requiere relabel (lento)
# No hay logs de AVC (no puedes diagnosticar nada)

# BIEN — usar permissive si necesitas desactivar temporalmente
SELINUX=permissive
# Archivos mantienen etiquetas
# Logs de AVC siguen generándose
# Volver a enforcing: solo setenforce 1
```

### Error 3: Olvidar que sestatus muestra DOS modos

```bash
sestatus
# Current mode:          permissive      ← runtime (setenforce)
# Mode from config file: enforcing       ← persistente (config)

# Esto significa: AHORA estamos en permissive,
# pero al reiniciar volveremos a enforcing

# Si no revisas ambos, puedes creer que el cambio es permanente
# cuando solo es temporal (o viceversa)
```

### Error 4: Re-activar SELinux sin relabel después de Disabled

```bash
# Pasos incorrectos:
# 1. Sistema estuvo con SELINUX=disabled por semanas
# 2. Se crearon/modificaron muchos archivos
# 3. Se cambia a SELINUX=enforcing
# 4. reboot

# Resultado: el sistema puede no arrancar o muchos servicios fallan
# porque archivos críticos tienen tipo unlabeled_t o default_t

# Pasos correctos:
# 1. Cambiar a SELINUX=permissive (no enforcing directamente)
SELINUX=permissive
# 2. Crear trigger de relabel
touch /.autorelabel
# 3. reboot (relabel ocurre, sistema arranca en permissive)
# 4. Verificar que no hay errores masivos en los logs
ausearch -m AVC --raw | audit2why | head -50
# 5. Si todo bien, cambiar a enforcing
SELINUX=enforcing
setenforce 1
```

### Error 5: Usar setenforce en un script de arranque del servicio

```bash
# MAL — desactivar SELinux como "fix" en un script de arranque
#!/bin/bash
setenforce 0          # "necesario para que funcione"
/opt/app/start.sh
# Resultado: TODO el sistema queda en permissive
# solo porque una app necesita ajustes de política

# BIEN — ajustar la política para la app
# O como mínimo, permissive solo para ese dominio:
semanage permissive -a myapp_t
```

---

## 9. Cheatsheet

```
=== TRES MODOS ===
Enforcing     política activa, violaciones denegadas + loggeadas
Permissive    política evaluada, violaciones loggeadas (no denegadas)
Disabled      SELinux desactivado, sin etiquetas, sin logs

=== CONSULTAR MODO ===
getenforce                         modo actual (una palabra)
sestatus                           información completa
  Current mode:                    modo runtime
  Mode from config file:           modo persistente (próximo boot)
cat /sys/fs/selinux/enforce        1=enforcing, 0=permissive

=== CAMBIAR MODO RUNTIME ===
setenforce 0                       → permissive (temporal)
setenforce 1                       → enforcing (temporal)
setenforce Permissive              → permissive (temporal)
setenforce Enforcing               → enforcing (temporal)
Solo alterna entre enforcing ↔ permissive, NO puede ir a disabled

=== CAMBIAR MODO PERSISTENTE ===
/etc/selinux/config
  SELINUX=enforcing|permissive|disabled
  SELINUXTYPE=targeted|minimum|mls
Requiere reboot para aplicar

=== PERMISSIVE POR DOMINIO ===
semanage permissive -a DOMINIO     poner un dominio en permissive
semanage permissive -d DOMINIO     quitar permissive de un dominio
semanage permissive -l             listar dominios en permissive

=== RE-ETIQUETADO ===
touch /.autorelabel                trigger relabel en próximo boot
fixfiles -F relabel                relabel manual (sin reboot)
restorecon -Rv /path/              relabel un directorio

=== PARÁMETROS DEL KERNEL ===
enforcing=0                        boot en permissive
enforcing=1                        boot en enforcing
selinux=0                          desactivar SELinux en este boot
autorelabel=1                      forzar relabel al arrancar

=== POLÍTICA ===
SELINUXTYPE=targeted               política estándar (confina servicios)
semodule -l                        listar módulos de política
sestatus | grep "Loaded policy"    ver política cargada

=== REGLA DE ORO ===
Nunca disabled → usar permissive si necesitas desactivar
Permissive por dominio > permissive global
setenforce 0 es temporal → verificar /etc/selinux/config
```

---

## 10. Ejercicios

### Ejercicio 1: Interpretar sestatus

Un compañero ejecuta `sestatus` en dos servidores y te muestra los resultados:

**Servidor A:**
```
SELinux status:                 enabled
SELinuxfs mount:                /sys/fs/selinux
Loaded policy name:             targeted
Current mode:                   permissive
Mode from config file:          enforcing
```

**Servidor B:**
```
SELinux status:                 enabled
SELinuxfs mount:                /sys/fs/selinux
Loaded policy name:             targeted
Current mode:                   enforcing
Mode from config file:          permissive
```

Para cada servidor:

1. ¿Qué modo está activo ahora?
2. ¿Qué modo se activará tras un reboot?
3. ¿Qué comando ejecutó alguien para causar esta discrepancia?
4. ¿Cuál de los dos servidores es más peligroso y por qué?
5. ¿Qué harías para corregir cada servidor?

> **Pregunta de predicción**: Si el servidor A se reinicia ahora sin que nadie toque nada, ¿volverá a enforcing o se quedará en permissive? ¿Podrían surgir problemas al volver a enforcing si alguien creó archivos mientras estaba en permissive?

### Ejercicio 2: Plan de reactivación

Heredas un servidor RHEL 8 que ha tenido `SELINUX=disabled` durante 2 años. El servidor corre Apache, PostgreSQL y una aplicación Java en Tomcat. El equipo de seguridad exige reactivar SELinux.

Escribe un plan paso a paso que:

1. Minimice el riesgo de caída del servicio
2. Identifique qué servicios necesitarán ajustes de política
3. Permita volver atrás si algo sale mal
4. Termine con SELinux en enforcing con todos los servicios funcionando

Para cada paso, indica el comando exacto y qué verificarías antes de continuar al siguiente.

> **Pregunta de predicción**: Durante el re-etiquetado, ¿los servicios estarán caídos? ¿Se puede hacer el relabel con el sistema en producción? ¿Qué impacto tiene `fixfiles relabel` en un servidor con 500 GB de datos?

### Ejercicio 3: Elegir el modo correcto

Para cada escenario, indica qué modo de SELinux usarías y justifica:

1. Servidor de producción con datos financieros (PCI-DSS)
2. Máquina de desarrollo de un programador junior
3. Servidor donde estás instalando una aplicación nueva por primera vez y no sabes si genera violaciones SELinux
4. Servidor de staging (pre-producción) que replica producción
5. Un contenedor Docker que ejecuta una app de terceros con instrucciones que dicen "desactivar SELinux"
6. Un servidor donde solo UN servicio nuevo (Grafana) tiene problemas con SELinux, pero el resto funciona perfecto

> **Pregunta de predicción**: Para el escenario 5, ¿por qué las instrucciones del vendor dicen "desactivar SELinux"? ¿Es porque la app es inherentemente incompatible, o hay una razón diferente? ¿Qué alternativa hay?

---

> **Siguiente tema**: T03 — Contextos SELinux (user:role:type:level, ls -Z, ps -Z, id -Z).
