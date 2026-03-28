# T06 — Primer contacto con SELinux/AppArmor

> **Fuentes**: `README.md`, `README.max.md`, `labs/README.md`
>
> **Correcciones aplicadas**:
> - Ejercicio 7 del max.md: `aa-status --profiled` no existe como flag válido.
>   Corregido.
> - Ejercicios 4/5 del max.md: comandos comentados con "descomenta para probar"
>   que no funcionarían en Docker de todas formas. Reescritos como ejercicios
>   más útiles.
> - Tabla "Desactivar": mezclaba `setenforce 0` (temporal) con edición del
>   config (permanente). Clarificado.
> - Descripción de la diferencia SELinux/AppArmor: "no por archivo como
>   SELinux" era impreciso. SELinux etiqueta TODO (archivos, procesos, puertos,
>   sockets). AppArmor usa reglas basadas en **rutas** vinculadas a programas
>   específicos. Clarificado.
> - Añadida mención de `restorecon` como el fix más común de SELinux
>   (cp/mv cambian el contexto).
> - Añadida distinción `chcon` (temporal) vs `semanage fcontext` (permanente).

---

## Por qué existen

Los permisos Unix (rwx, ACLs) son **DAC** — *Discretionary Access Control*.
El propietario del archivo decide quién tiene acceso. El problema: si un
proceso es comprometido, tiene **todos** los permisos del usuario que lo
ejecuta. Si ese usuario es root, el atacante tiene acceso total.

**MAC** — *Mandatory Access Control* — agrega una segunda capa de permisos
que el kernel aplica **independientemente de los permisos Unix**. Incluso
root puede ser bloqueado si la política MAC no permite el acceso.

```
DAC (permisos Unix):       ¿El USUARIO tiene permiso sobre este archivo?
MAC (SELinux/AppArmor):    ¿Este PROGRAMA tiene permiso sobre este recurso?

Ambos deben permitir el acceso para que se conceda.
```

MAC limita lo que puede hacer **cada programa**, no solo cada usuario. Un
servidor web comprometido no podrá leer `/etc/shadow` aunque los permisos
Unix se lo permitan.

### Cada distribución usa un MAC diferente

| Distribución | MAC | Activo por defecto |
|---|---|---|
| RHEL/AlmaLinux/Fedora | SELinux | Sí (enforcing) |
| Debian/Ubuntu | AppArmor | Sí (enforce) |
| SUSE/openSUSE | AppArmor | Sí (enforce) |

Ambos son **LSMs** (*Linux Security Modules*) — módulos del kernel que
interceptan llamadas al sistema antes de que accedan a recursos.

---

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
# Loaded policy name:             targeted
# Current mode:                   enforcing
# Mode from config file:          enforcing
```

### Modos de SELinux

| Modo | Bloquea | Registra | Uso |
|---|---|---|---|
| Enforcing | Sí | Sí | Producción |
| Permissive | No | Sí | Debugging — ve qué bloquearía sin romper nada |
| Disabled | No | No | No recomendado — re-habilitar requiere relabel completo |

```bash
# Cambiar temporalmente (hasta el próximo reinicio)
sudo setenforce 0    # → Permissive
sudo setenforce 1    # → Enforcing

# Cambiar permanentemente
sudo vi /etc/selinux/config
# SELINUX=enforcing
```

> **Nota:** Pasar de Disabled a Enforcing requiere un *relabel* completo del
> filesystem (el sistema crea un archivo `/.autorelabel` y reinicia). Puede
> tomar mucho tiempo en discos grandes.

### Contextos de seguridad

SELinux asigna un **contexto** (etiqueta) a **todo**: archivos, procesos,
puertos, sockets. El formato es `user:role:type:level`:

```bash
# Ver contextos de archivos
ls -Z /etc/passwd
# system_u:object_r:passwd_file_t:s0  /etc/passwd
#                   ^^^^^^^^^^^^^^
#                   tipo del archivo

# Ver contextos de procesos
ps -eZ | grep httpd
# system_u:system_r:httpd_t:s0     1234 ?  ... /usr/sbin/httpd
#                   ^^^^^^^
#                   tipo del proceso
```

El kernel verifica: "¿puede un proceso con tipo `httpd_t` acceder a un
archivo con tipo `httpd_sys_content_t`?" La política dice que sí. Pero si
httpd intenta acceder a un archivo con tipo `user_home_t`, la política lo
bloquea — aunque los permisos Unix lo permitan.

El campo **type** es el más importante. Es lo que las políticas usan para
decidir acceso ("*type enforcement*").

### El error más común y su solución

El síntoma típico: "funciona sin SELinux pero falla con SELinux":

```bash
# Ejemplo: mover archivos web a una ubicación custom
mv /home/dev/index.html /srv/web/

# ls -Z muestra que el archivo conservó el contexto de /home/:
ls -Z /srv/web/index.html
# unconfined_u:object_r:user_home_t:s0    ← tipo incorrecto!
# httpd espera: httpd_sys_content_t

# nginx no puede servirlo → AVC denied en audit.log
```

**`restorecon` es el fix más común.** Restablece los contextos de archivos
según la política:

```bash
# Fix rápido: restablecer contextos
sudo restorecon -Rv /srv/web/
# Relabeled /srv/web/index.html from user_home_t to httpd_sys_content_t

# Si la ruta no tiene regla en la política, agregarla primero:
sudo semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
sudo restorecon -Rv /srv/web/
```

### chcon vs semanage fcontext

| Comando | Persistencia | Uso |
|---|---|---|
| `chcon -t tipo archivo` | **Temporal** — el próximo `restorecon` lo revierte | Pruebas rápidas |
| `semanage fcontext -a -t tipo "ruta(/.*)?"` + `restorecon` | **Permanente** — queda en la base de datos de políticas | Producción |

### ¿Por qué cp y mv causan problemas?

- **`cp`** crea un archivo nuevo → hereda el contexto del directorio destino.
  Generalmente **no** causa problemas.
- **`mv`** mueve el inodo → **conserva** el contexto del directorio origen.
  Esto causa la mayoría de problemas de SELinux.

### Herramientas de diagnóstico rápido

```bash
# Ver los últimos bloqueos de SELinux
sudo ausearch -m AVC --start recent

# Obtener sugerencias de corrección (muy útil)
sudo ausearch -m AVC --start recent | audit2why

# sealert — análisis detallado (paquete setroubleshoot)
sudo sealert -a /var/log/audit/audit.log
```

---

## AppArmor (Debian/Ubuntu)

### Verificar el estado

```bash
sudo aa-status
# apparmor module is loaded.
# 28 profiles are loaded.
# 26 profiles are in enforce mode.
#    /usr/bin/evince
#    /usr/sbin/cupsd
#    ...
# 2 profiles are in complain mode.
#    /usr/sbin/named
#    ...
```

### Perfiles de AppArmor

AppArmor usa **perfiles por programa** con reglas basadas en **rutas de
archivos** (no etiquetas como SELinux):

```bash
# Ver perfiles disponibles
ls /etc/apparmor.d/
# usr.sbin.cupsd
# usr.sbin.named
# ...

# Contenido de un perfil
cat /etc/apparmor.d/usr.sbin.cupsd
# /usr/sbin/cupsd {
#   #include <abstractions/base>
#   capability net_bind_service,
#   /etc/cups/** r,          ← puede leer archivos de config
#   /var/spool/cups/** rw,   ← puede leer/escribir el spool
#   deny /etc/shadow r,      ← explícitamente prohibido
# }
```

**Diferencia clave con SELinux:** AppArmor no necesita etiquetar archivos.
Las reglas usan rutas directamente (`/etc/cups/**`). Esto lo hace más simple
pero menos granular — si un archivo se mueve, la ruta cambia y las reglas
pueden dejar de aplicar.

### Modos de AppArmor

| Modo | Equivalente SELinux | Comportamiento |
|---|---|---|
| Enforce | Enforcing | Bloquea y registra |
| Complain | Permissive | Solo registra (no bloquea) |
| Disabled (por perfil) | — | El perfil no se carga |

```bash
# Cambiar un perfil a complain (para diagnóstico)
sudo aa-complain /usr/sbin/named

# Volver a enforce
sudo aa-enforce /usr/sbin/named

# Desactivar un perfil
sudo aa-disable /usr/sbin/named
```

Los cambios de modo son **por perfil**, no globales (a diferencia de
`setenforce` que cambia todo SELinux).

### Logs de AppArmor

```bash
# En sistemas con journalctl:
sudo journalctl -k | grep apparmor

# En sistemas con syslog:
sudo grep -i apparmor /var/log/syslog

# Ejemplo de denegación:
# apparmor="DENIED" operation="open" profile="/usr/sbin/named"
#   name="/etc/shadow" pid=1234 comm="named"
```

---

## Comparación rápida

| Aspecto | SELinux | AppArmor |
|---|---|---|
| Distribución | RHEL, Fedora | Debian, Ubuntu, SUSE |
| Modelo | Etiquetas en TODO (archivos, procesos, puertos) | Perfiles por programa con rutas |
| Complejidad | Mayor — políticas complejas | Menor — reglas de rutas legibles |
| Mover archivos | Puede causar problemas (contexto viaja con el archivo) | No causa problemas (reglas son por ruta) |
| Granularidad | Muy fina (puertos, sockets, IPC) | Suficiente para la mayoría de casos |
| Ver estado | `getenforce`, `sestatus` | `aa-status` |
| Ver contextos | `ls -Z`, `ps -Z`, `id -Z` | N/A (basado en rutas, no etiquetas) |
| Debugging | `ausearch -m AVC`, `audit2why` | `journalctl -k`, `grep apparmor syslog` |
| Configuración | `/etc/selinux/config` | `/etc/apparmor.d/` |

---

## Qué necesitas saber ahora

En este punto del curso, solo necesitas saber:

1. **Que existe**: si algo falla misteriosamente en permisos, puede ser MAC
2. **Cómo verificar**: `getenforce` (RHEL) o `aa-status` (Debian)
3. **Cómo diagnosticar**: mirar los logs antes de desactivar
4. **No desactivar como primera opción**: `setenforce 0` y `aa-complain` son
   para diagnóstico, no para producción
5. **`restorecon`** (SELinux): el fix más común. Si moviste archivos y un
   servicio falla, prueba `restorecon -Rv /ruta/`

La configuración completa de SELinux y AppArmor se cubre en **B11** (Seguridad,
Kernel y Arranque).

---

## Labs

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

> **Nota importante:** Dentro de contenedores Docker, SELinux y AppArmor
> generalmente **no están activos** porque operan a nivel del kernel del host,
> no del contenedor. Los labs exploran las herramientas y conceptos, pero
> algunos comandos mostrarán "no disponible". Esto es esperado.

---

### Parte 1 — DAC vs MAC

#### Paso 1.1: Qué son DAC y MAC

```bash
docker compose exec debian-dev bash -c '
echo "=== DAC (Discretionary Access Control) ==="
echo "- Permisos Unix tradicionales (rwx, ACLs)"
echo "- El propietario decide quién accede"
echo "- Si un proceso es comprometido, tiene todos los permisos del usuario"

echo ""
echo "=== MAC (Mandatory Access Control) ==="
echo "- Segunda capa, independiente de DAC"
echo "- El sistema (política) decide qué puede hacer cada PROGRAMA"
echo "- Incluso root puede ser bloqueado"
echo "- SELinux (RHEL) o AppArmor (Debian)"

echo ""
echo "=== Ambos deben permitir el acceso ==="
echo "DAC permite? + MAC permite? → acceso concedido"
echo "Cualquiera bloquea → acceso denegado"
'
```

#### Paso 1.2: Por qué importa

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario sin MAC ==="
echo "1. nginx ejecuta como www-data"
echo "2. Vulnerabilidad en nginx permite ejecución de código"
echo "3. El atacante tiene TODOS los permisos de www-data"
echo "4. Puede leer cualquier archivo que www-data pueda leer"

echo ""
echo "=== Escenario con MAC ==="
echo "1. nginx ejecuta como www-data"
echo "2. Vulnerabilidad en nginx permite ejecución de código"
echo "3. MAC limita nginx a SOLO sus archivos específicos"
echo "4. El atacante no puede leer /etc/shadow ni otros archivos"
echo "   aunque los permisos Unix lo permitirían"
'
```

---

### Parte 2 — SELinux (AlmaLinux)

#### Paso 2.1: Estado de SELinux

```bash
docker compose exec alma-dev bash -c '
echo "=== getenforce ==="
getenforce 2>/dev/null || echo "(SELinux no disponible en este contenedor)"

echo ""
echo "=== sestatus ==="
sestatus 2>/dev/null || echo "(sestatus no disponible)"
echo ""
echo "(En Docker, SELinux opera a nivel del host, no del contenedor)"
'
```

#### Paso 2.2: Contextos de archivos

```bash
docker compose exec alma-dev bash -c '
echo "=== Contextos de archivos con ls -Z ==="
ls -Z /etc/passwd 2>/dev/null || echo "(SELinux no disponible)"
ls -Z /etc/shadow 2>/dev/null
ls -Z /usr/bin/passwd 2>/dev/null

echo ""
echo "Formato: user:role:type:level"
echo "El campo type es el más importante para las políticas"
'
```

#### Paso 2.3: Contextos de procesos

```bash
docker compose exec alma-dev bash -c '
echo "=== Contextos de procesos ==="
ps -eZ 2>/dev/null | head -10 || echo "(ps -Z no disponible)"

echo ""
echo "=== Contexto de tu shell ==="
id -Z 2>/dev/null || echo "(id -Z no disponible)"
'
```

#### Paso 2.4: Herramientas disponibles

```bash
docker compose exec alma-dev bash -c '
echo "=== Herramientas de SELinux ==="
for cmd in getenforce sestatus setenforce restorecon ausearch audit2why semanage; do
    if command -v $cmd &>/dev/null; then
        echo "  $cmd: disponible"
    else
        echo "  $cmd: NO disponible"
    fi
done
'
```

---

### Parte 3 — AppArmor (Debian)

#### Paso 3.1: Estado de AppArmor

```bash
docker compose exec debian-dev bash -c '
echo "=== aa-status ==="
aa-status 2>/dev/null || echo "(AppArmor no disponible en este contenedor)"

echo ""
echo "=== Kernel module cargado? ==="
cat /sys/module/apparmor/parameters/enabled 2>/dev/null || echo "(no disponible)"
echo ""
echo "(En Docker, AppArmor opera a nivel del host, no del contenedor)"
'
```

#### Paso 3.2: Perfiles de AppArmor

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorio de perfiles ==="
ls /etc/apparmor.d/ 2>/dev/null | head -10 || echo "(directorio no existe en contenedor)"

echo ""
echo "=== Ejemplo de perfil ==="
echo "AppArmor define perfiles POR PROGRAMA con reglas de RUTAS:"
echo ""
echo "/usr/sbin/cupsd {"
echo "  #include <abstractions/base>"
echo "  capability net_bind_service,"
echo "  /etc/cups/** r,           ← puede leer config"
echo "  /var/spool/cups/** rw,    ← puede leer/escribir spool"
echo "  deny /etc/shadow r,       ← explícitamente prohibido"
echo "}"
'
```

#### Paso 3.3: Herramientas disponibles

```bash
docker compose exec debian-dev bash -c '
echo "=== Herramientas de AppArmor ==="
for cmd in aa-status aa-enforce aa-complain aa-disable apparmor_parser; do
    if command -v $cmd &>/dev/null; then
        echo "  $cmd: disponible"
    else
        echo "  $cmd: NO disponible"
    fi
done
'
```

#### Paso 3.4: Comparación rápida

```bash
docker compose exec debian-dev bash -c '
printf "%-20s %-25s %-25s\n" "Aspecto" "SELinux" "AppArmor"
printf "%-20s %-25s %-25s\n" "----------" "----------" "----------"
printf "%-20s %-25s %-25s\n" "Distribución" "RHEL, Fedora" "Debian, Ubuntu, SUSE"
printf "%-20s %-25s %-25s\n" "Modelo" "Etiquetas en todo" "Perfiles con rutas"
printf "%-20s %-25s %-25s\n" "Complejidad" "Mayor" "Menor"
printf "%-20s %-25s %-25s\n" "Ver estado" "getenforce, sestatus" "aa-status"
printf "%-20s %-25s %-25s\n" "Configuración" "/etc/selinux/" "/etc/apparmor.d/"
'
```

---

### Limpieza final

No hay recursos que limpiar.

---

## Ejercicios

### Ejercicio 1 — Detectar qué MAC tiene el sistema

```bash
echo "=== Detección automática ==="
if command -v getenforce &>/dev/null; then
    echo "SELinux detectado"
    echo "Estado: $(getenforce 2>/dev/null || echo 'no disponible')"
    sestatus 2>/dev/null | head -5
elif command -v aa-status &>/dev/null; then
    echo "AppArmor detectado"
    aa-status 2>/dev/null | head -5
else
    echo "No se detectó SELinux ni AppArmor"
fi
```

**Pregunta:** ¿Tu sistema tiene SELinux, AppArmor, o ninguno? ¿Está activo?
¿En qué modo?

<details><summary>Predicción</summary>

Depende de la distribución:

- **RHEL/AlmaLinux/Fedora**: SELinux detectado. En un sistema real estaría en
  `Enforcing` con política `targeted`. En un contenedor Docker, puede
  mostrar `Disabled` o no estar disponible.

- **Debian/Ubuntu**: AppArmor detectado. En un sistema real mostraría perfiles
  cargados en enforce/complain. En un contenedor Docker, puede no estar
  disponible.

- **Container Docker**: Generalmente "no disponible" para ambos. SELinux y
  AppArmor operan a nivel del **kernel del host**, no del contenedor. El
  contenedor hereda las restricciones del host pero no puede consultar
  directamente el estado.

En un host Fedora/RHEL con Docker: los contenedores están protegidos por
SELinux del host (tipo `container_t`), aunque dentro del contenedor
`getenforce` no funcione.

</details>

---

### Ejercicio 2 — Contextos SELinux: ls -Z y ps -Z

```bash
# Ejecutar en AlmaLinux (o cualquier sistema con SELinux):
ls -Z /etc/passwd /etc/shadow /usr/bin/passwd 2>/dev/null
echo "---"
ps -eZ 2>/dev/null | head -5
echo "---"
id -Z 2>/dev/null
```

**Pregunta:** ¿Qué significan los campos `user:role:type:level`? ¿Cuál es
el campo más importante para las políticas de acceso?

<details><summary>Predicción</summary>

Ejemplo de salida (en un sistema RHEL real):
```
system_u:object_r:passwd_file_t:s0   /etc/passwd
system_u:object_r:shadow_t:s0        /etc/shadow
system_u:object_r:passwd_exec_t:s0   /usr/bin/passwd
```

Los cuatro campos del contexto:
- **user** (`system_u`): usuario SELinux (no es el usuario Unix). Define qué
  roles puede asumir.
- **role** (`object_r`): rol SELinux. Para archivos siempre es `object_r`.
  Para procesos varía (`system_r`, `unconfined_r`).
- **type** (`passwd_file_t`): **el campo más importante**. Las políticas de
  acceso (*type enforcement*) se basan en este campo. Define qué tipos de
  procesos pueden acceder a qué tipos de archivos.
- **level** (`s0`): nivel MLS (*Multi-Level Security*). En la política
  `targeted` (la más común), siempre es `s0`.

El **type** es lo que importa en la práctica:
- `passwd_file_t` → archivo de contraseñas
- `shadow_t` → archivo shadow (más restringido)
- `passwd_exec_t` → ejecutable passwd (transiciona a `passwd_t` al ejecutar)
- `httpd_sys_content_t` → contenido web servible por httpd

</details>

---

### Ejercicio 3 — AppArmor: perfiles y estado

```bash
# Ejecutar en Debian/Ubuntu:
aa-status 2>/dev/null | head -20
echo "---"
ls /etc/apparmor.d/ 2>/dev/null | head -10
```

**Pregunta:** ¿Cuántos perfiles están en enforce vs complain? ¿Los nombres
de los archivos de perfil te dicen algo sobre qué programa protegen?

<details><summary>Predicción</summary>

Ejemplo de salida (en un sistema Debian real):
```
apparmor module is loaded.
28 profiles are loaded.
26 profiles are in enforce mode.
   /usr/bin/evince
   /usr/sbin/cups-browsed
   /usr/sbin/cupsd
   ...
2 profiles are in complain mode.
   ...
```

```
ls /etc/apparmor.d/:
usr.sbin.cupsd
usr.sbin.named
usr.bin.evince
...
```

Los nombres de los archivos de perfil usan puntos en lugar de `/` para
representar la ruta del programa:
- `usr.sbin.cupsd` → perfil para `/usr/sbin/cupsd`
- `usr.bin.evince` → perfil para `/usr/bin/evince`

Esta convención de nombres hace evidente qué programa protege cada perfil.
Los perfiles en **enforce** bloquean y registran violaciones. Los perfiles
en **complain** solo registran sin bloquear (útil para desarrollo de
nuevos perfiles).

</details>

---

### Ejercicio 4 — DAC vs MAC: cuándo sospechar de MAC

Escenario: Un administrador configura nginx para servir archivos desde
`/srv/web/`. Los permisos Unix son correctos:

```
drwxr-xr-x root:root /srv/web/
-rw-r--r-- root:root /srv/web/index.html
```

Nginx ejecuta como `www-data`. Los permisos others (`r-x` y `r--`) permiten
lectura. Pero nginx devuelve **403 Forbidden**.

**Pregunta:** ¿Qué checarías? ¿Cómo confirmarías si es MAC?

<details><summary>Predicción</summary>

**Checklist de diagnóstico:**

1. **Verificar permisos DAC** (ya están bien en este caso):
   ```bash
   ls -la /srv/web/
   namei -l /srv/web/index.html    # verificar toda la ruta
   ```

2. **Verificar MAC:**
   ```bash
   # SELinux (RHEL):
   getenforce                                    # ¿Está en Enforcing?
   ls -Z /srv/web/index.html                     # ¿Contexto correcto?
   sudo ausearch -m AVC --start recent           # ¿Hay denegaciones?
   sudo ausearch -m AVC --start recent | audit2why  # ¿Sugerencias?

   # AppArmor (Debian):
   aa-status                                     # ¿nginx tiene perfil?
   sudo journalctl -k | grep -i "apparmor.*nginx"  # ¿Denegaciones?
   ```

3. **Confirmar cambiando a modo permisivo temporalmente:**
   ```bash
   # SELinux:
   sudo setenforce 0       # si nginx funciona ahora → era SELinux
   sudo setenforce 1       # restaurar inmediatamente

   # AppArmor:
   sudo aa-complain nginx  # si nginx funciona → era AppArmor
   sudo aa-enforce nginx   # restaurar
   ```

4. **Solución en SELinux (la más probable en RHEL):**
   ```bash
   ls -Z /srv/web/index.html
   # probablemente muestra default_t o user_home_t en lugar de httpd_sys_content_t
   sudo semanage fcontext -a -t httpd_sys_content_t "/srv/web(/.*)?"
   sudo restorecon -Rv /srv/web/
   ```

**Regla rápida:** Si los permisos Unix están bien y aún hay 403/Permission
denied → sospechar de MAC como primera opción.

</details>

---

### Ejercicio 5 — SELinux: interpretar un mensaje AVC

Dado este mensaje de audit.log:

```
type=AVC msg=audit(1679503200.123:456): avc: denied { read } for
  pid=2345 comm="httpd" name="config.yml" dev="sda2" ino=789012
  scontext=system_u:system_r:httpd_t:s0
  tcontext=unconfined_u:object_r:user_home_t:s0
  tclass=file permissive=0
```

**Pregunta:** ¿Qué proceso fue bloqueado? ¿Qué intentó hacer? ¿Qué tipo
tiene el archivo? ¿Cuál debería tener? ¿Cómo lo arreglarías?

<details><summary>Predicción</summary>

Desglose del mensaje AVC:

| Campo | Valor | Significado |
|---|---|---|
| `denied { read }` | read | Intentó **leer** un archivo |
| `comm="httpd"` | httpd | El proceso que fue bloqueado |
| `name="config.yml"` | config.yml | El archivo al que intentó acceder |
| `scontext=...httpd_t:s0` | httpd_t | Tipo del proceso (source context) |
| `tcontext=...user_home_t:s0` | user_home_t | Tipo del archivo (target context) |
| `permissive=0` | 0 | SELinux está en Enforcing (bloqueó de verdad) |

**Diagnóstico:** httpd (tipo `httpd_t`) intentó leer `config.yml` que tiene
tipo `user_home_t`. La política no permite que `httpd_t` lea archivos con
tipo `user_home_t`.

**El archivo tiene tipo incorrecto.** Probablemente fue copiado o movido
desde un directorio home. Debería tener `httpd_sys_content_t`.

**Solución:**
```bash
# Si el archivo está en una ruta estándar de httpd:
sudo restorecon -v /path/to/config.yml

# Si está en una ruta custom:
sudo semanage fcontext -a -t httpd_sys_content_t "/path/to(/.*)?"
sudo restorecon -Rv /path/to/
```

</details>

---

### Ejercicio 6 — AppArmor: interpretar una denegación

Dado este mensaje de log:

```
kernel: audit: type=1400 audit(1679503200.456:789):
  apparmor="DENIED" operation="open" profile="/usr/sbin/named"
  name="/home/admin/zones/example.com.zone" pid=3456
  comm="named" requested_mask="r" denied_mask="r"
```

**Pregunta:** ¿Qué programa fue bloqueado? ¿Qué archivo intentó abrir?
¿Por qué fue bloqueado? ¿Cómo lo arreglarías?

<details><summary>Predicción</summary>

Desglose del mensaje:

| Campo | Valor | Significado |
|---|---|---|
| `apparmor="DENIED"` | DENIED | Fue bloqueado (no solo registrado) |
| `operation="open"` | open | Intentó abrir un archivo |
| `profile="/usr/sbin/named"` | named | El perfil que lo bloqueó |
| `name="/home/admin/zones/..."` | zones/example.com.zone | El archivo |
| `requested_mask="r"` | r | Intentó leer |

**Diagnóstico:** `named` (DNS server) intentó leer un archivo de zona desde
`/home/admin/zones/`. El perfil de AppArmor para named probablemente solo
permite leer de `/etc/bind/`, `/var/cache/bind/`, y ubicaciones estándar.
`/home/admin/` no está en la lista permitida.

**Soluciones (de mejor a peor):**

1. **Mover los archivos de zona a la ubicación estándar:**
   ```bash
   sudo mv /home/admin/zones/ /etc/bind/zones/
   ```

2. **Agregar la ruta al perfil de AppArmor:**
   ```bash
   # Editar /etc/apparmor.d/usr.sbin.named
   # Agregar: /home/admin/zones/** r,
   sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.named
   ```

3. **Poner el perfil en complain (solo para diagnóstico):**
   ```bash
   sudo aa-complain /usr/sbin/named
   ```

La opción 1 es la mejor porque respeta el principio de least surprise.

</details>

---

### Ejercicio 7 — ¿Por qué mv causa problemas con SELinux pero no con AppArmor?

```bash
# Escenario conceptual:
# 1. Crear archivo en /home/dev/
# 2. Moverlo a /var/www/html/
# 3. httpd intenta leerlo
```

**Pregunta:** ¿Por qué este escenario causa problemas con SELinux pero no
con AppArmor?

<details><summary>Predicción</summary>

**Con SELinux:**
```bash
touch /home/dev/page.html          # contexto: user_home_t
mv /home/dev/page.html /var/www/html/  # mv CONSERVA el contexto
ls -Z /var/www/html/page.html
# user_home_t   ← INCORRECTO, debería ser httpd_sys_content_t
# httpd bloqueado → AVC denied
```

`mv` preserva el inodo (solo actualiza la entrada del directorio), y el
contexto SELinux está almacenado con el inodo. El archivo llega a su
destino con el contexto equivocado.

Fix: `sudo restorecon -v /var/www/html/page.html`

**Con AppArmor:**
```bash
touch /home/dev/page.html
mv /home/dev/page.html /var/www/html/
# httpd puede leerlo si su perfil permite /var/www/html/** r
```

AppArmor usa **rutas**, no etiquetas. No importa de dónde vino el archivo —
lo que importa es **dónde está ahora**. Si `/var/www/html/**` está en la
lista de rutas permitidas del perfil de httpd, el acceso se concede.

**Resumen:**
- SELinux: la identidad del archivo viaja con él (etiqueta en el inodo)
- AppArmor: la identidad depende de dónde está (ruta en el filesystem)

Esto es una ventaja y desventaja de cada enfoque:
- SELinux es más seguro (un archivo malicioso no gana permisos solo por estar
  en la ruta correcta)
- AppArmor es más predecible (mover archivos no rompe cosas)

</details>

---

### Ejercicio 8 — restorecon: el fix más usado en SELinux

```bash
# Ejecutar en AlmaLinux (conceptual si SELinux no está activo):
# Verificar qué contexto DEBERÍA tener un archivo según la política
matchpathcon /etc/passwd 2>/dev/null || echo "(matchpathcon no disponible)"
matchpathcon /var/www/html/index.html 2>/dev/null || echo "(no disponible)"

# restorecon -n (dry run): muestra qué cambiaría sin cambiar nada
restorecon -nRv /etc/ 2>/dev/null | head -5 || echo "(no disponible)"
```

**Pregunta:** ¿Qué hace `restorecon`? ¿Por qué `-n` es útil antes de
ejecutarlo realmente?

<details><summary>Predicción</summary>

`restorecon` restablece los contextos de seguridad SELinux de archivos a
los valores que la política define para esas rutas. Consulta la base de
datos de `semanage fcontext` y aplica los contextos correctos.

```
matchpathcon /etc/passwd:
  /etc/passwd    system_u:object_r:passwd_file_t:s0

matchpathcon /var/www/html/index.html:
  /var/www/html/index.html    system_u:object_r:httpd_sys_content_t:s0
```

**`-n` (dry run)** muestra qué archivos tienen contextos incorrectos y qué
cambiaría, sin modificar nada. Es útil para:

1. Verificar si hay archivos con contextos incorrectos sin arriesgarse
2. Entender el alcance del cambio antes de aplicarlo
3. Diagnosticar sin tener permisos de escritura

**Uso típico:**
```bash
restorecon -nRv /srv/web/    # ver qué cambiaría
restorecon -Rv /srv/web/     # aplicar los cambios
```

`restorecon` es el comando más usado en la resolución de problemas de
SELinux. Si algo falla y sospechas de SELinux, es lo primero que pruebas.

</details>

---

### Ejercicio 9 — Comparar MAC en los dos containers

```bash
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
if command -v getenforce &>/dev/null; then
    echo "MAC: SELinux"
    echo "Estado: $(getenforce 2>/dev/null || echo "no disponible")"
else
    echo "SELinux: herramientas no disponibles"
fi
'

echo ""
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
if command -v aa-status &>/dev/null; then
    echo "MAC: AppArmor"
    aa-status 2>/dev/null | head -3 || echo "Estado: no disponible"
else
    echo "AppArmor: herramientas no disponibles"
fi
'
```

**Pregunta:** ¿Ambos containers tienen sus herramientas MAC instaladas?
¿Están activos? ¿Por qué o por qué no?

<details><summary>Predicción</summary>

**AlmaLinux:**
```
MAC: SELinux
Estado: Disabled (o "no disponible")
```
Las herramientas (`getenforce`, `sestatus`) probablemente están instaladas
porque RHEL las incluye por defecto. Pero SELinux está `Disabled` o no
funcional dentro del container.

**Debian:**
```
MAC: AppArmor
Estado: no disponible
```
Las herramientas (`aa-status`) pueden o no estar instaladas. AppArmor no
funciona dentro del container.

**¿Por qué no funcionan en Docker?**

Tanto SELinux como AppArmor operan a nivel del **kernel del host**. Los
containers comparten el kernel del host y no pueden cargar módulos de
seguridad propios. El host puede usar SELinux/AppArmor para restringir
los containers (Docker aplica perfiles por defecto), pero dentro del
container no se puede consultar ni modificar el estado.

En un servidor real (no containerizado), ambos estarían activos por defecto
y se verificaría con `getenforce` o `aa-status` respectivamente.

</details>

---

### Ejercicio 10 — Checklist de diagnóstico: ¿es MAC?

Un servicio devuelve "Permission denied" pero los permisos Unix están
correctos. Escribe los pasos que seguirías para diagnosticar.

**Pregunta:** ¿Cuál es tu checklist?

<details><summary>Predicción</summary>

**Checklist de diagnóstico "¿es MAC?":**

```
1. VERIFICAR PERMISOS DAC (descartar lo obvio)
   □ ls -la /ruta/al/archivo
   □ namei -l /ruta/al/archivo    (toda la cadena de directorios)
   □ ¿El servicio corre como qué usuario? (ps aux | grep servicio)

2. VERIFICAR QUÉ MAC HAY
   □ RHEL: getenforce → ¿Enforcing?
   □ Debian: aa-status → ¿Perfiles en enforce?

3. BUSCAR DENEGACIONES EN LOGS
   □ SELinux: sudo ausearch -m AVC --start recent
   □ AppArmor: sudo journalctl -k | grep apparmor
   □ Generic: sudo dmesg | grep -i "denied\|apparmor\|avc"

4. CONFIRMAR (modo permisivo temporal)
   □ SELinux: sudo setenforce 0 → probar → sudo setenforce 1
   □ AppArmor: sudo aa-complain perfil → probar → sudo aa-enforce perfil
   □ ¿Funciona ahora? → ERA MAC

5. SOLUCIONAR (no dejar en permisivo)
   □ SELinux: verificar contextos con ls -Z
     → restorecon -Rv /ruta/
     → o semanage fcontext + restorecon
   □ AppArmor: agregar ruta al perfil
     → o mover archivos a ubicación estándar

6. VERIFICAR
   □ Restaurar enforce/enforcing
   □ Probar que el servicio funciona
   □ Verificar logs: no más denegaciones
```

**Regla de oro:** NUNCA dejar MAC desactivado como "solución". Si desactivas
MAC y "funciona", ya confirmaste el problema — ahora arréglalo correctamente
y reactiva MAC.

</details>

---

## Limpieza de ejercicios

No hay recursos que limpiar — todos los ejercicios son conceptuales o
de consulta.
