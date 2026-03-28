# Crear perfiles: aa-genprof, aa-logprof y sintaxis

## Índice

1. [Estrategias para crear perfiles](#1-estrategias-para-crear-perfiles)
2. [Sintaxis completa de perfiles](#2-sintaxis-completa-de-perfiles)
3. [aa-genprof: generación guiada](#3-aa-genprof-generación-guiada)
4. [aa-logprof: refinamiento iterativo](#4-aa-logprof-refinamiento-iterativo)
5. [Crear perfiles manualmente](#5-crear-perfiles-manualmente)
6. [Abstractions y tunables](#6-abstractions-y-tunables)
7. [Sub-perfiles y transiciones](#7-sub-perfiles-y-transiciones)
8. [Probar y depurar perfiles](#8-probar-y-depurar-perfiles)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Estrategias para crear perfiles

Hay tres formas de crear un perfil de AppArmor, de más automatizada a más
manual:

```
Métodos para crear perfiles
═════════════════════════════

  1. aa-genprof (generación guiada)
     ─────────────────────────────
     → Crea un perfil inicial vacío en complain
     → Ejecutas el programa
     → aa-genprof analiza los logs y te pregunta qué permitir
     → Resultado: perfil funcional generado interactivamente
     → Mejor para: servicios que no conoces bien

  2. aa-logprof (refinamiento iterativo)
     ───────────────────────────────────
     → Trabaja con un perfil ya existente (en complain)
     → Analiza los logs de violaciones ALLOWED
     → Te pregunta qué reglas agregar
     → Mejor para: ajustar perfiles existentes

  3. Manual
     ──────
     → Escribes el perfil desde cero
     → Control total sobre cada regla
     → Mejor para: servicios que conoces a fondo
     → Requiere entender la sintaxis completa
```

```
Flujo recomendado
══════════════════

  ¿Conoces bien el servicio?
  │
  ├── NO → aa-genprof (generación guiada)
  │          │
  │          ▼
  │        Perfil inicial generado
  │          │
  │          ▼
  │        aa-logprof (refinar si faltan accesos)
  │
  └── SÍ → Perfil manual + complain + aa-logprof para pulir
```

---

## 2. Sintaxis completa de perfiles

Antes de usar las herramientas de generación, necesitas entender qué producen:

### Estructura general

```
# Comentario — el parser los ignora
#include <tunables/global>              ← Variables globales

/ruta/del/ejecutable flags=(attach_disconnected) {
  #include <abstractions/base>          ← Permisos comunes

  # === Capabilities ===
  capability net_bind_service,
  capability dac_override,

  # === Network ===
  network inet tcp,
  network inet udp,
  network inet6 tcp,

  # === Signals ===
  signal send set=(term, kill) peer=/usr/sbin/nginx,

  # === File rules ===
  /etc/nginx/** r,
  /var/log/nginx/*.log w,
  /var/www/html/** r,
  /run/nginx.pid rw,

  # === Execute rules ===
  /usr/bin/cat ix,

  # === Local overrides ===
  #include <local/usr.sbin.nginx>
}
```

### Reglas de archivo en detalle

```
Sintaxis de reglas de archivo
══════════════════════════════

  Formato:
    [qualifier] ruta permisos,
                │    │        │
                │    │        └── Coma obligatoria
                │    └── Uno o más: r w a l k m ix px cx ux
                └── Ruta absoluta con globs opcionales


  Permisos de lectura/escritura:
  ──────────────────────────────
  r   read         Leer contenido del archivo
  w   write        Escribir, crear, truncar, eliminar
  a   append       Solo agregar al final (logs)
  l   link         Crear hardlinks al archivo
  k   lock         flock() sobre el archivo
  m   mmap exec    Memory-map con permisos de ejecución


  Permisos de ejecución (mutuamente excluyentes):
  ─────────────────────────────────────────────────
  ix    inherit       Ejecutar heredando el perfil actual
  px    profile       Ejecutar con su propio perfil (debe existir)
  Px    clean profile Como px pero limpia variables de entorno
  cx    child         Ejecutar con un sub-perfil definido dentro
  Cx    clean child   Como cx pero limpia variables de entorno
  ux    unconfined    Ejecutar sin confinamiento (¡evitar!)
  Ux    clean unconf  Como ux pero limpia variables de entorno


  Ejemplos:
  ─────────
  /etc/passwd r,                      Leer /etc/passwd
  /var/log/app/*.log w,               Escribir en cualquier .log
  /var/lib/app/** rw,                 Leer/escribir recursivamente
  /var/lib/app/ r,                    Listar el directorio
  /usr/bin/helper px,                 Ejecutar con perfil propio
  /usr/lib/app/scripts/*.sh ix,       Ejecutar heredando perfil
  owner /home/*/.config/app/** rw,    Solo propietario puede r/w
```

### Qualifier: owner

```
Qualifier owner
════════════════

  owner /tmp/app-*.sock rw,
  │
  └── Solo permite el acceso si el UID del proceso
      coincide con el UID del propietario del archivo

  Sin owner:
    /tmp/app-*.sock rw,
    → Cualquier proceso con este perfil puede leer/escribir
      el socket (si los permisos DAC lo permiten)

  Con owner:
    owner /tmp/app-*.sock rw,
    → Solo si el proceso es el DUEÑO del socket
```

### Reglas de red

```
Reglas de red (network)
════════════════════════

  Formato:
    network [domain] [type],

  Dominios:          Tipos:
    inet   (IPv4)      stream  (TCP)
    inet6  (IPv6)      dgram   (UDP)
    unix   (local)     raw     (raw sockets)
    netlink             seqpacket

  Ejemplos:
    network,                     Todo tráfico de red
    network inet,                Todo IPv4
    network inet tcp,            Solo TCP sobre IPv4
    network inet6 udp,           UDP sobre IPv6
    network unix,                Sockets Unix
    network inet stream,         Equivale a "inet tcp"
    network inet dgram,          Equivale a "inet udp"

  Reglas de socket Unix (más granulares):
    unix (send, receive) type=stream peer=(label=/usr/sbin/mysqld),
```

### Capabilities

```
Capabilities del kernel
════════════════════════

  Formato:
    capability nombre,

  Capabilities comunes:
    capability net_bind_service,   Bind a puertos < 1024
    capability dac_override,       Ignorar permisos de archivo
    capability dac_read_search,    Leer/buscar ignorando permisos
    capability setuid,             Cambiar UID
    capability setgid,             Cambiar GID
    capability chown,              Cambiar propietario
    capability kill,               Enviar señales
    capability sys_ptrace,         ptrace() otros procesos
    capability net_raw,            Sockets raw (tcpdump)
    capability net_admin,          Configuración de red
    capability sys_admin,          Operaciones de admin

  Solo incluir las que el servicio necesite — mínimo privilegio.
```

### Señales

```
Reglas de señales
══════════════════

  Formato:
    signal [permisos] [set=señales] [peer=perfil],

  Permisos: send, receive
  Señales: term, kill, hup, int, usr1, usr2, etc.

  Ejemplos:
    signal,                                  Todas las señales
    signal send,                             Enviar cualquier señal
    signal send set=(term, kill),            Solo SIGTERM y SIGKILL
    signal send set=(hup) peer=/usr/sbin/nginx,  HUP a nginx
    signal receive set=(term) peer=unconfined,   Recibir TERM de no confinados
```

### Deny rules

```
Deny explícito
════════════════

  Por defecto, todo lo que NO está en el perfil está denegado.
  Pero puedes usar deny para:

  1. Anular una regla más amplia:
     /etc/** r,                  Leer todo en /etc/
     deny /etc/shadow r,         Excepto shadow

  2. Silenciar logs:
     deny /proc/*/status r,      Denegar sin generar log
                                  (útil para accesos frecuentes
                                   que llenarían el log)

  Sin deny: acceso bloqueado → log generado (DENIED)
  Con deny: acceso bloqueado → SIN log (silencioso)
```

---

## 3. aa-genprof: generación guiada

`aa-genprof` crea un perfil nuevo de forma interactiva, observando el
comportamiento real del programa:

### Flujo de trabajo

```
aa-genprof — flujo completo
═════════════════════════════

  Terminal 1:                        Terminal 2:
  ──────────                         ──────────
  $ sudo aa-genprof /usr/sbin/myapp
  │
  ├── Crea perfil vacío en complain
  │   /etc/apparmor.d/usr.sbin.myapp
  │
  ├── Carga perfil en kernel         $ sudo systemctl start myapp
  │                                  $ curl http://localhost:8080
  │   "Please start the application  $ myapp --test-feature
  │    in another window and          (ejercitar TODAS las
  │    exercise its functionality."    funcionalidades)
  │
  │   Presiona (S)can
  │         │
  │         ▼
  │   Lee los logs de ALLOWED
  │   y te pregunta regla por regla:
  │
  │   Profile: /usr/sbin/myapp
  │   Path:    /etc/myapp/config.yaml
  │   Mode:    r
  │
  │   [1 - /etc/myapp/config.yaml]    ← Ruta exacta
  │   [2 - /etc/myapp/*]              ← Glob un nivel
  │   [3 - /etc/myapp/**]             ← Glob recursivo
  │   (A)llow / (D)eny / (I)gnore / (G)lob / (N)ew
  │
  │   Eliges opción para cada acceso
  │         │
  │         ▼
  │   Presiona (F)inish cuando hayas
  │   cubierto toda la funcionalidad
  │         │
  │         ▼
  └── Guarda perfil y lo carga en enforce
```

### Ejemplo práctico

```bash
# Terminal 1: iniciar generación
sudo aa-genprof /usr/sbin/nginx

# Salida:
# Writing updated profile for /usr/sbin/nginx.
# Setting /usr/sbin/nginx to complain mode.
#
# Please start the application to be profiled in
# another window and exercise its functionality now.
#
# Once completed, select the "Scan" option below for
# AppArmor to review the log events.
#
# Profiling: /usr/sbin/nginx
#
# [(S)can system log for AppArmor events] /
# [(F)inish]
```

```bash
# Terminal 2: ejercitar nginx
sudo systemctl start nginx
curl http://localhost/
curl https://localhost/
curl http://localhost/nonexistent  # probar 404
```

```bash
# Terminal 1: presionar S para escanear

# Pregunta 1:
# Profile:  /usr/sbin/nginx
# Path:     /etc/nginx/nginx.conf
# Mode:     r
#
# [1 - /etc/nginx/nginx.conf]
# [2 - /etc/nginx/*]
# (A)llow / (D)eny / ...

# Respuesta: elegir 2 (permitir leer cualquier archivo en /etc/nginx/)
# → Genera: /etc/nginx/* r,

# Pregunta 2:
# Path:     /var/log/nginx/access.log
# Mode:     w
#
# [1 - /var/log/nginx/access.log]
# [2 - /var/log/nginx/*]
# (A)llow / (D)eny / ...

# Respuesta: elegir 2
# → Genera: /var/log/nginx/* w,
```

### Opciones durante aa-genprof

```
Opciones al revisar cada acceso
═════════════════════════════════

  (A)llow    → Permitir con la ruta seleccionada (número)
  (D)eny     → Denegar explícitamente (agrega deny rule)
  (I)gnore   → Ignorar este acceso (no agregar regla)
  (G)lob     → Ampliar el glob (más permisivo)
  (N)ew      → Escribir una ruta personalizada
  (S)ave     → Guardar cambios hasta ahora

  Para ejecución de otros programas:
  (I)nherit  → ix (heredar perfil)
  (C)hild    → cx (sub-perfil)
  (P)rofile  → px (perfil propio)
  (U)nconfined → ux (sin confinar — evitar)
```

### Consejos para aa-genprof

```
Consejos para obtener buenos perfiles
══════════════════════════════════════

  1. Ejercitar TODA la funcionalidad
     → No solo el caso normal
     → Incluir errores, recargas, rotación de logs
     → Ejemplo nginx: reload, logrotate, error pages

  2. Preferir globs moderados
     → /etc/nginx/* en vez de /etc/nginx/nginx.conf
     → /var/www/html/** en vez de archivos individuales
     → Pero NO /etc/** (demasiado amplio)

  3. Hacer múltiples pasadas de Scan
     → Ejercitar → Scan → ejercitar más → Scan
     → Cada pasada captura accesos diferentes

  4. No usar (U)nconfined para subprocesos
     → Preferir (I)nherit o (P)rofile
     → ux rompe la cadena de confinamiento

  5. Revisar el perfil generado antes de confiar
     → cat /etc/apparmor.d/usr.sbin.nginx
     → ¿Hay reglas demasiado amplias?
```

---

## 4. aa-logprof: refinamiento iterativo

`aa-logprof` es similar a `aa-genprof` pero trabaja con perfiles **ya
existentes**. Analiza los logs y propone reglas para accesos que el perfil
actual no cubre:

### Cuándo usar aa-logprof

```
aa-genprof vs aa-logprof
═════════════════════════

  aa-genprof:
    → Crear perfil NUEVO desde cero
    → Pone en complain automáticamente
    → Interactivo (espera que ejercites el programa)
    → Resultado: perfil nuevo

  aa-logprof:
    → Ajustar perfil EXISTENTE
    → El perfil ya debe estar en complain (tú lo pones)
    → No es interactivo (lee logs acumulados)
    → Resultado: reglas adicionales al perfil existente

  Flujo típico:
    1. aa-genprof  → perfil inicial
    2. aa-enforce  → producción
    3. (algo falla → DENIED en logs)
    4. aa-complain → temporalmente
    5. (ejercitar la funcionalidad que fallaba)
    6. aa-logprof  → agregar reglas faltantes
    7. aa-enforce  → volver a producción
```

### Uso

```bash
# 1. El perfil debe estar en complain
sudo aa-complain /usr/sbin/nginx

# 2. Ejercitar la funcionalidad que falla
sudo systemctl restart nginx
# ... usar la funcionalidad problemática ...

# 3. Ejecutar aa-logprof (analiza los logs)
sudo aa-logprof

# Muestra las mismas preguntas que aa-genprof:
# Profile:  /usr/sbin/nginx
# Path:     /srv/new-site/index.html
# Mode:     r
# [1 - /srv/new-site/index.html]
# [2 - /srv/new-site/*]
# [3 - /srv/new-site/**]
# (A)llow / (D)eny / ...

# 4. Responder las preguntas

# 5. Volver a enforce
sudo aa-enforce /usr/sbin/nginx
```

### aa-logprof con filtros

```bash
# Solo analizar logs desde cierta fecha
sudo aa-logprof -d /var/log/audit/audit.log

# Solo para un perfil específico
# (no hay flag directo, pero puedes filtrar logs antes)
ausearch -m APPARMOR_ALLOWED -c nginx --raw > /tmp/nginx-logs.txt
sudo aa-logprof -f /tmp/nginx-logs.txt
```

---

## 5. Crear perfiles manualmente

Para servicios que conoces bien, escribir el perfil a mano da más control:

### Plantilla base

```
#include <tunables/global>

/usr/sbin/myservice {
  #include <abstractions/base>

  # Configuración
  /etc/myservice/ r,
  /etc/myservice/** r,

  # Datos
  /var/lib/myservice/ r,
  /var/lib/myservice/** rw,

  # Logs
  /var/log/myservice/ r,
  /var/log/myservice/*.log w,
  /var/log/myservice/*.log.* r,    # Logs rotados

  # PID / Socket
  /run/myservice/ r,
  /run/myservice/** rw,

  # Red
  network inet tcp,
  network inet6 tcp,

  # Capabilities
  capability net_bind_service,

  # Customizaciones locales
  #include <local/usr.sbin.myservice>
}
```

### Proceso paso a paso

```bash
# 1. Crear el archivo de perfil
sudo nano /etc/apparmor.d/usr.sbin.myservice
# (escribir perfil usando la plantilla arriba)

# 2. Crear el archivo local (vacío pero necesario para el include)
sudo touch /etc/apparmor.d/local/usr.sbin.myservice

# 3. Verificar sintaxis
sudo apparmor_parser -p /etc/apparmor.d/usr.sbin.myservice
# Si hay errores, los muestra con número de línea

# 4. Cargar en modo complain para probar
sudo aa-complain /usr/sbin/myservice

# 5. Ejercitar el servicio
sudo systemctl start myservice
# ... usar todas las funcionalidades ...

# 6. Revisar logs por accesos que faltan
journalctl -k --since "10 minutes ago" | grep ALLOWED | grep myservice

# 7. Agregar reglas faltantes al perfil

# 8. Recargar
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.myservice

# 9. Repetir 5-8 hasta que no haya ALLOWED inesperados

# 10. Pasar a enforce
sudo aa-enforce /usr/sbin/myservice
```

### Ejemplo: perfil para una aplicación Flask con Gunicorn

```
#include <tunables/global>

/usr/bin/gunicorn {
  #include <abstractions/base>
  #include <abstractions/nameservice>
  #include <abstractions/python>

  # Intérprete Python
  /usr/bin/python3.* ix,

  # Código de la aplicación
  /opt/myflaskapp/ r,
  /opt/myflaskapp/** r,
  /opt/myflaskapp/**/__pycache__/** rw,

  # Entorno virtual
  /opt/myflaskapp/venv/** r,
  /opt/myflaskapp/venv/bin/* ix,

  # Configuración
  /opt/myflaskapp/.env r,

  # Uploads (lectura/escritura)
  /opt/myflaskapp/uploads/** rw,

  # Logs
  /var/log/gunicorn/ r,
  /var/log/gunicorn/*.log w,

  # Socket o TCP
  /run/gunicorn/ r,
  /run/gunicorn/myapp.sock rw,
  network inet tcp,          # Si escucha en TCP en vez de socket

  # PID
  /run/gunicorn/myapp.pid rw,

  # Temporales
  owner /tmp/gunicorn-* rw,

  # Denegar explícitamente accesos peligrosos
  deny /etc/shadow r,
  deny /root/** rw,

  #include <local/usr.bin.gunicorn>
}
```

---

## 6. Abstractions y tunables

### Abstractions más usadas

```bash
# Ver todas las abstractions disponibles
ls /etc/apparmor.d/abstractions/
```

```
Abstractions esenciales
════════════════════════

  base
  ────
  Casi siempre se incluye. Proporciona:
    /etc/ld.so.cache r,        Linker
    /lib/**.so* r,             Bibliotecas compartidas
    /dev/null rw,              /dev/null y /dev/zero
    /dev/urandom r,            Números aleatorios
    /proc/sys/kernel/** r,     Info del kernel
    → Incluir siempre como primera línea


  nameservice
  ───────────
  Para servicios que resuelven nombres:
    /etc/hosts r,
    /etc/resolv.conf r,
    /etc/nsswitch.conf r,
    /etc/gai.conf r,
    /run/systemd/resolve/** r,
    → Incluir si el servicio hace DNS o consulta usuarios


  authentication
  ──────────────
  Para servicios que autentican usuarios:
    /etc/pam.d/** r,
    /etc/shadow r,
    /etc/security/** r,
    → Solo para servicios de login/auth (sshd, ftpd)


  openssl / ssl_certs / ssl_keys
  ──────────────────────────────
  openssl:    funciones de OpenSSL
  ssl_certs:  leer certificados (/etc/ssl/certs/**)
  ssl_keys:   leer claves privadas (/etc/ssl/private/**)
    → Incluir para servicios que usan TLS


  python
  ──────
  Acceso a módulos Python, bytecode cache:
    /usr/lib/python3/** r,
    /usr/lib/python3/**/__pycache__/** rw,
    → Incluir para aplicaciones Python


  apache2-common
  ──────────────
  Compartido entre perfiles de Apache:
    Módulos, configuración, MIME types
    → Solo para Apache
```

### Tunables: variables en perfiles

```bash
# Ver tunables disponibles
ls /etc/apparmor.d/tunables/

# global  ← Cargado por defecto en todos los perfiles
# home    ← Definición de @{HOME}
# proc    ← Definición de @{PROC}
```

```
Variables tunables
═══════════════════

  El archivo tunables/global define:
    @{PROC} = /proc/
    @{HOME} = /home/ /root/
    @{HOMEDIRS} = /home/

  Uso en perfiles:
    owner @{HOME}/.config/myapp/** rw,
    → Se expande a:
    owner /home/*/.config/myapp/** rw,
    owner /root/.config/myapp/** rw,

    @{PROC}/sys/kernel/random/uuid r,
    → Se expande a:
    /proc/sys/kernel/random/uuid r,

  Personalizar tunables:
    # Agregar un directorio home adicional
    echo '@{HOME} += /data/home/' >> /etc/apparmor.d/tunables/home.d/mysite
```

---

## 7. Sub-perfiles y transiciones

### Sub-perfiles (child profiles)

Un perfil puede contener sub-perfiles para programas que ejecuta:

```
Perfil con sub-perfil
══════════════════════

  /usr/sbin/nginx {
    #include <abstractions/base>

    /etc/nginx/** r,
    /var/www/html/** r,

    # Ejecutar helper con sub-perfil
    /usr/lib/nginx/helper cx -> helper,

    # Definición del sub-perfil
    profile helper /usr/lib/nginx/helper {
      #include <abstractions/base>
      /var/cache/nginx/** rw,
      /tmp/nginx-* rw,
    }
  }

  Cuando nginx ejecuta /usr/lib/nginx/helper:
    → El helper corre con el sub-perfil "helper"
    → Tiene sus propios permisos (más restringidos)
    → No hereda todos los permisos de nginx
```

### Hat sub-profiles (cambio de contexto)

Los hats permiten que un programa cambie de perfil **dentro de sí mismo**
usando `change_hat()`:

```
Hat profiles — cambio de contexto
═══════════════════════════════════

  /usr/sbin/apache2 {
    #include <abstractions/base>

    # Permisos generales de Apache
    /etc/apache2/** r,
    capability net_bind_service,

    # Hat para cada VirtualHost
    ^site1 {
      /var/www/site1/** r,
      /var/log/apache2/site1-*.log w,
    }

    ^site2 {
      /var/www/site2/** r,
      /var/log/apache2/site2-*.log w,
    }
  }

  Apache usa change_hat() con mod_apparmor:
    → Petición a site1.com → ^site1 (solo puede leer /var/www/site1/)
    → Petición a site2.com → ^site2 (solo puede leer /var/www/site2/)
    → Aislamiento entre VirtualHosts dentro del mismo proceso
```

### Transiciones de ejecución

```
Resumen de transiciones al ejecutar un programa
═════════════════════════════════════════════════

  Regla          Comportamiento
  ────           ──────────────
  /path ix       Hereda el perfil del padre
                 → El hijo tiene los MISMOS permisos
                 → Útil para scripts helper simples

  /path px       Transiciona al perfil de /path
                 → El perfil /path { } debe existir
                 → Separación de privilegios

  /path Px       Como px + limpia variables de entorno
                 → Más seguro (evita LD_PRELOAD tricks)

  /path cx       Transiciona a un sub-perfil (child)
                 → El sub-perfil está DENTRO del perfil padre
                 → Más fácil de gestionar (todo en un archivo)

  /path ux       Ejecuta SIN confinamiento
                 → ¡PELIGROSO! Rompe la cadena MAC
                 → Solo como último recurso

  /path Ux       Como ux + limpia variables de entorno
                 → Sigue siendo peligroso, solo ligeramente menos

  Regla general: ix > px > cx >> ux
  (de más seguro a menos seguro)
```

---

## 8. Probar y depurar perfiles

### Verificación de sintaxis

```bash
# Verificar sin cargar
sudo apparmor_parser -p /etc/apparmor.d/usr.sbin.myservice

# Errores comunes de sintaxis:
# AppArmor parser error at line 15: syntax error, unexpected TOK_ID
# → Falta una coma al final de la regla
#
# AppArmor parser error at line 8: Could not open 'abstractions/bse'
# → Typo en el nombre de la abstraction
#
# AppArmor parser error: Invalid capability 'net_bing_service'
# → Typo en el nombre del capability
```

### Flujo de depuración

```
Depuración paso a paso
════════════════════════

  1. Verificar sintaxis
     sudo apparmor_parser -p PERFIL
       │
       ├── Error → Corregir sintaxis
       │
       └── OK → Continuar
       │
       ▼
  2. Cargar en complain
     sudo aa-complain /usr/sbin/myservice
       │
       ▼
  3. Ejecutar servicio y ejercitar funcionalidades
     sudo systemctl restart myservice
       │
       ▼
  4. Revisar logs
     journalctl -k --since "5 min ago" | grep apparmor
       │
       ├── ALLOWED → Agregar regla o deny explícito
       │
       └── Sin mensajes → Todo está cubierto
       │
       ▼
  5. Recargar perfil
     sudo apparmor_parser -r PERFIL
       │
       ▼
  6. Repetir 3-5 hasta satisfecho
       │
       ▼
  7. Pasar a enforce
     sudo aa-enforce /usr/sbin/myservice
       │
       ▼
  8. Monitorear en producción
     journalctl -k -f | grep DENIED
```

### aa-notify: alertas en tiempo real

```bash
# Instalar (si no viene por defecto)
sudo apt install apparmor-notify    # Debian/Ubuntu

# Monitorear denegaciones en tiempo real
sudo aa-notify -s 1 --display $DISPLAY

# En terminal (sin escritorio):
sudo aa-notify -s 1 -v
#   -s 1  → verificar cada 1 segundo
#   -v    → modo verbose

# Salida:
# Profile: /usr/sbin/nginx
# Operation: open
# Name: /etc/forbidden-file
# Denied: r
```

### aa-decode: decodificar mensajes de audit

```bash
# Los logs de audit pueden tener campos codificados en hex
# aa-decode los traduce

# Mensaje con hex:
# apparmor="DENIED" name=2F746D702F746573742E747874

# Decodificar:
echo '2F746D702F746573742E747874' | aa-decode
# /tmp/test.txt
```

---

## 9. Errores comunes

### Error 1: olvidar la coma al final de las reglas

```
# MAL — falta la coma
/etc/myapp/** r

# BIEN
/etc/myapp/** r,
               ^
               └── La coma es OBLIGATORIA en cada regla

# Error que verás:
# AppArmor parser error at line X: syntax error
```

### Error 2: no incluir la abstraction base

```
# MAL — perfil sin abstractions/base
/usr/sbin/myservice {
  /etc/myservice/** r,
  /var/lib/myservice/** rw,
}
# → El servicio no puede cargar bibliotecas compartidas
# → No puede acceder a /dev/null, /dev/urandom
# → Casi nada funciona

# BIEN
/usr/sbin/myservice {
  #include <abstractions/base>       ← Incluir SIEMPRE
  /etc/myservice/** r,
  /var/lib/myservice/** rw,
}
```

### Error 3: confundir * con **

```
# /var/log/myapp/*     → solo archivos en /var/log/myapp/
#                         NO incluye subdirectorios
#
# /var/log/myapp/**    → archivos en /var/log/myapp/ Y
#                         todos los subdirectorios recursivamente

# Error común: usar * cuando hay subdirectorios
/etc/nginx/* r,
# → NO lee /etc/nginx/conf.d/default.conf  (está en subdirectorio)

# Correcto:
/etc/nginx/** r,
# → Sí lee /etc/nginx/conf.d/default.conf
```

### Error 4: no crear el archivo local/ vacío

```bash
# El perfil incluye:
#   #include <local/usr.sbin.myservice>
# Pero el archivo no existe:

sudo apparmor_parser -p /etc/apparmor.d/usr.sbin.myservice
# AppArmor parser error: Could not open 'local/usr.sbin.myservice'

# Solución: crear el archivo vacío
sudo touch /etc/apparmor.d/local/usr.sbin.myservice
```

### Error 5: usar ux en vez de ix o px

```
# MAL — ejecutar helper sin confinar
/usr/lib/myapp/helper ux,
# → El helper puede hacer CUALQUIER cosa
# → Rompe completamente la cadena de seguridad

# BIEN — heredar perfil del padre
/usr/lib/myapp/helper ix,
# → El helper tiene los mismos permisos que el padre

# MEJOR — perfil propio con permisos mínimos
/usr/lib/myapp/helper px,
# → El helper tiene su propio perfil restringido
# (requiere crear el perfil para /usr/lib/myapp/helper)
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║           Crear perfiles AppArmor — Cheatsheet                 ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  HERRAMIENTAS                                                    ║
║  sudo aa-genprof /usr/sbin/X   Crear perfil nuevo (interactivo) ║
║  sudo aa-logprof               Refinar perfil existente          ║
║  sudo aa-notify -s 1 -v        Alertas de denegación en vivo    ║
║  aa-decode HEX                 Decodificar campos hex en logs    ║
║                                                                  ║
║  PERMISOS DE ARCHIVO                                             ║
║  r=read  w=write  a=append  l=link  k=lock  m=mmap             ║
║                                                                  ║
║  PERMISOS DE EJECUCIÓN (mejor → peor)                           ║
║  ix = inherit (hereda perfil padre)                              ║
║  px = profile (transiciona a perfil propio)                      ║
║  cx = child   (transiciona a sub-perfil)                        ║
║  ux = unconfined (¡evitar!)                                     ║
║  Px/Cx/Ux = igual + limpiar entorno                             ║
║                                                                  ║
║  GLOBS                                                           ║
║  *   = un nivel (sin subdirectorios)                             ║
║  **  = recursivo (con subdirectorios)                            ║
║                                                                  ║
║  PLANTILLA MÍNIMA                                                ║
║  #include <tunables/global>                                      ║
║  /usr/sbin/X {                                                   ║
║    #include <abstractions/base>                                  ║
║    /etc/X/** r,                                                  ║
║    /var/lib/X/** rw,                                             ║
║    /var/log/X/*.log w,                                           ║
║    /run/X/** rw,                                                 ║
║    network inet tcp,                                             ║
║    #include <local/usr.sbin.X>                                   ║
║  }                                                               ║
║                                                                  ║
║  ABSTRACTIONS CLAVE                                              ║
║  base          → Siempre incluir (libc, /dev/null)              ║
║  nameservice   → Si hace DNS o consulta usuarios                ║
║  authentication → Si autentica (PAM, shadow)                    ║
║  ssl_certs     → Si usa TLS (leer certificados)                 ║
║  python        → Si es aplicación Python                        ║
║                                                                  ║
║  FLUJO COMPLETO                                                  ║
║  1. aa-genprof o escribir perfil manual                          ║
║  2. apparmor_parser -p (verificar sintaxis)                     ║
║  3. aa-complain (probar sin bloquear)                           ║
║  4. Ejercitar servicio                                           ║
║  5. aa-logprof o revisar logs manualmente                       ║
║  6. Agregar reglas faltantes                                     ║
║  7. apparmor_parser -r (recargar)                               ║
║  8. Repetir 4-7                                                  ║
║  9. aa-enforce (producción)                                     ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Analizar un perfil existente

```bash
# Leer el perfil de tcpdump (suele ser sencillo)
cat /etc/apparmor.d/usr.sbin.tcpdump
```

> **Pregunta de predicción**: sin leer el archivo, ¿qué capabilities
> esperarías ver en el perfil de tcpdump? ¿Qué tipo de regla de red
> necesitaría?
>
> **Respuesta**: `capability net_raw` (para capturar paquetes con raw
> sockets), probablemente `capability net_admin` (para configurar modo
> promiscuo), y `network raw` o `network packet` (para sockets de captura).
> También esperarías reglas de escritura para la ruta donde guarda
> las capturas (`/tmp/` o un directorio especificado con `-w`).

### Ejercicio 2: Escribir un perfil mínimo

**Escenario**: tienes un servicio `/usr/local/bin/logwatcher` que:
- Lee configuración de `/etc/logwatcher/config.yml`
- Lee archivos de log de `/var/log/**`
- Escribe reportes en `/var/lib/logwatcher/reports/`
- No necesita red
- No necesita capabilities especiales

```
# Escribe el perfil antes de ver la solución
```

> **Pregunta de predicción**: ¿necesitas incluir `abstractions/nameservice`
> para este servicio? ¿Por qué sí o por qué no?
>
> **Respuesta**: probablemente no. `nameservice` es para servicios que
> resuelven DNS o consultan usuarios. Un logwatcher que solo lee archivos
> locales y escribe reportes no necesita resolver nombres. Sin embargo, si
> el servicio enviara alertas por email o hiciera peticiones HTTP, sí la
> necesitaría. La solución:

```
#include <tunables/global>

/usr/local/bin/logwatcher {
  #include <abstractions/base>

  /etc/logwatcher/ r,
  /etc/logwatcher/** r,

  /var/log/ r,
  /var/log/** r,

  /var/lib/logwatcher/reports/ r,
  /var/lib/logwatcher/reports/** rw,

  #include <local/usr.local.bin.logwatcher>
}
```

### Ejercicio 3: Diagnosticar un perfil incompleto

**Escenario**: nginx tiene un perfil en enforce pero no puede servir archivos
desde un nuevo directorio `/opt/sites/production/`.

```bash
# Paso 1: Verificar los DENIED
journalctl -k --since "5 minutes ago" | grep DENIED | grep nginx

# Salida esperada:
# apparmor="DENIED" operation="open"
#   profile="/usr/sbin/nginx"
#   name="/opt/sites/production/index.html"
#   requested_mask="r" denied_mask="r"
```

> **Pregunta de predicción**: ¿agregarías la regla al perfil principal
> (`/etc/apparmor.d/usr.sbin.nginx`) o al archivo local
> (`/etc/apparmor.d/local/usr.sbin.nginx`)? ¿Qué pasos exactos seguirías
> después de agregar la regla?
>
> **Respuesta**: al archivo local (`/etc/apparmor.d/local/usr.sbin.nginx`)
> para que sobreviva actualizaciones del paquete. La regla sería:
> `/opt/sites/production/** r,`. Los pasos después:
> 1. `apparmor_parser -p /etc/apparmor.d/usr.sbin.nginx` (verificar sintaxis)
> 2. `apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx` (recargar)
> 3. No necesitas reiniciar nginx en este caso — las nuevas reglas del perfil
>    se aplican inmediatamente a los procesos ya confinados bajo ese perfil.

---

> **Siguiente tema**: T04 — SELinux vs AppArmor (comparación práctica, cuándo
> encontrar cada uno).
