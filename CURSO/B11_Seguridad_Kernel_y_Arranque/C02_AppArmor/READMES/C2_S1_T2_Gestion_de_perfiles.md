# Gestión de perfiles: aa-enforce, aa-complain, aa-disable

## Índice

1. [Ciclo de vida de un perfil](#1-ciclo-de-vida-de-un-perfil)
2. [apparmor_parser: el motor de carga](#2-apparmor_parser-el-motor-de-carga)
3. [aa-enforce: activar protección](#3-aa-enforce-activar-protección)
4. [aa-complain: modo aprendizaje](#4-aa-complain-modo-aprendizaje)
5. [aa-disable: deshabilitar perfiles](#5-aa-disable-deshabilitar-perfiles)
6. [Gestión masiva de perfiles](#6-gestión-masiva-de-perfiles)
7. [Recarga y actualización de perfiles](#7-recarga-y-actualización-de-perfiles)
8. [Personalización con local/](#8-personalización-con-local)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Ciclo de vida de un perfil

Un perfil de AppArmor pasa por estados definidos. Entender el ciclo completo
es clave para gestionarlos correctamente:

```
Ciclo de vida de un perfil AppArmor
═════════════════════════════════════

                    ┌──────────────────┐
                    │  Archivo en disco │
                    │  /etc/apparmor.d/ │
                    └────────┬─────────┘
                             │
                    apparmor_parser -a
                    (o systemctl start apparmor)
                             │
                             ▼
               ┌─────────────────────────┐
          ┌────│   Perfil cargado en     │────┐
          │    │   memoria del kernel    │    │
          │    └─────────────────────────┘    │
          │              │                    │
    aa-enforce      aa-complain         aa-disable
          │              │              (apparmor_parser -R)
          ▼              ▼                    │
    ┌──────────┐  ┌────────────┐              ▼
    │ enforce  │  │  complain  │     ┌──────────────┐
    │ (bloquea │  │ (registra  │     │  descargado  │
    │  + log)  │  │  sin       │     │  (no activo) │
    └──────────┘  │  bloquear) │     └──────────────┘
                  └────────────┘

  Transiciones posibles:
    enforce  ↔  complain     (aa-enforce / aa-complain)
    enforce  →  descargado   (aa-disable)
    complain →  descargado   (aa-disable)
    descargado → enforce     (aa-enforce)
    descargado → complain    (aa-complain)
```

### Los tres niveles de acción

```
Tres niveles: disco, kernel, proceso
══════════════════════════════════════

  1. DISCO: archivo en /etc/apparmor.d/
     → Define las reglas
     → Puede existir sin estar cargado

  2. KERNEL: perfil cargado en memoria
     → El kernel lo aplica a nuevos procesos
     → Puede estar en enforce o complain
     → apparmor_parser gestiona la carga/descarga

  3. PROCESO: programa ejecutándose
     → Hereda el perfil al ejecutarse (exec)
     → Un proceso ya corriendo NO cambia de perfil
       automáticamente al recargar*
     → Necesita reiniciar el servicio para aplicar cambios

  * Excepción: cambiar enforce↔complain sí afecta procesos
    ya corriendo, porque el modo se evalúa en cada acceso.
```

> **Punto sutil**: cambiar un perfil de enforce a complain afecta
> inmediatamente a los procesos ya corriendo bajo ese perfil. Pero si
> **cambias las reglas** del perfil (agregas/quitas permisos), necesitas
> recargar con `apparmor_parser -r` Y reiniciar el servicio.

---

## 2. apparmor_parser: el motor de carga

`apparmor_parser` es la herramienta de bajo nivel que interactúa directamente
con el módulo del kernel. Las herramientas `aa-*` son wrappers sobre ella.

### Operaciones fundamentales

```bash
# Cargar (add) un perfil nuevo
apparmor_parser -a /etc/apparmor.d/usr.sbin.nginx
#               ─┬─
#                └── add: cargar perfil que no está en el kernel

# Recargar (replace) un perfil existente
apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
#               ─┬─
#                └── replace: recargar con reglas actualizadas

# Descargar (remove) un perfil del kernel
apparmor_parser -R /etc/apparmor.d/usr.sbin.nginx
#               ─┬─
#                └── Remove: quitar perfil de la memoria del kernel
```

### Flags adicionales útiles

```bash
# Verificar sintaxis sin cargar (dry run)
apparmor_parser -p /etc/apparmor.d/usr.sbin.nginx
#               ─┬─
#                └── parse: solo verificar, no cargar

# También se puede usar --debug para más detalle
apparmor_parser -p --debug /etc/apparmor.d/usr.sbin.nginx

# Cargar con caché (más rápido en arranque)
apparmor_parser -a --write-cache /etc/apparmor.d/usr.sbin.nginx

# Recargar TODOS los perfiles del directorio
apparmor_parser -r /etc/apparmor.d/
```

### Verificar qué perfiles están cargados en el kernel

```bash
# Lista de perfiles en el kernel (interfaz de bajo nivel)
cat /sys/kernel/security/apparmor/profiles

# Ejemplo de salida:
# /usr/sbin/nginx (enforce)
# /usr/sbin/mysqld (enforce)
# /usr/sbin/tcpdump (enforce)
# /usr/sbin/dnsmasq (complain)

# Más legible con aa-status
sudo aa-status
```

### Flujo de apparmor_parser

```
apparmor_parser — flujo de procesamiento
══════════════════════════════════════════

  Archivo en disco
  /etc/apparmor.d/usr.sbin.nginx
       │
       ▼
  1. Leer archivo
       │
       ▼
  2. Procesar #include
     (expandir abstractions, tunables, local/)
       │
       ▼
  3. Validar sintaxis
     (¿rutas válidas? ¿permisos reconocidos? ¿llaves balanceadas?)
       │
       ├── Error de sintaxis → mensaje de error, no carga
       │
       ▼
  4. Compilar a formato binario del kernel
       │
       ▼
  5. Enviar al módulo del kernel vía securityfs
     (/sys/kernel/security/apparmor/.load)
       │
       ▼
  Perfil activo en el kernel
```

---

## 3. aa-enforce: activar protección

`aa-enforce` pone un perfil en modo enforce (bloquea y registra):

### Sintaxis

```bash
# Por ruta del ejecutable
sudo aa-enforce /usr/sbin/nginx

# Por ruta del archivo de perfil
sudo aa-enforce /etc/apparmor.d/usr.sbin.nginx

# Múltiples perfiles
sudo aa-enforce /usr/sbin/nginx /usr/sbin/mysqld /usr/sbin/tcpdump
```

### Qué hace internamente

```
aa-enforce — acciones internas
════════════════════════════════

  sudo aa-enforce /usr/sbin/nginx
       │
       ▼
  1. Determinar archivo de perfil
     /usr/sbin/nginx → /etc/apparmor.d/usr.sbin.nginx
       │
       ▼
  2. ¿Existe symlink en /etc/apparmor.d/disable/?
     │
     ├── SÍ → Eliminar symlink (rehabilitar perfil)
     │
     └── NO → Continuar
       │
       ▼
  3. ¿Existe symlink en /etc/apparmor.d/force-complain/?
     │
     ├── SÍ → Eliminar symlink
     │
     └── NO → Continuar
       │
       ▼
  4. Cargar/recargar perfil en modo enforce
     apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
       │
       ▼
  Setting /etc/apparmor.d/usr.sbin.nginx to enforce mode.
```

### Ejemplo práctico

```bash
# Un perfil está en complain y queremos pasarlo a enforce
sudo aa-status | grep complain
#   /usr/sbin/dnsmasq

sudo aa-enforce /usr/sbin/dnsmasq
# Setting /etc/apparmor.d/usr.sbin.dnsmasq to enforce mode.

# Verificar
sudo aa-status | grep dnsmasq
#   /usr/sbin/dnsmasq    ← ahora aparece en la lista de enforce
```

### enforce y procesos ya corriendo

```
¿Qué pasa con procesos ya corriendo?
══════════════════════════════════════

  Caso 1: cambio de complain → enforce
  ─────────────────────────────────────
  El proceso ya corriendo INMEDIATAMENTE empieza a ser
  bloqueado por las reglas. No necesita reiniciar.

  Caso 2: perfil estaba deshabilitado → enforce
  ──────────────────────────────────────────────
  El perfil se carga en el kernel, PERO los procesos
  que ya estaban corriendo sin perfil NO se confinan.
  Necesitas reiniciar el servicio:

    sudo aa-enforce /usr/sbin/nginx
    sudo systemctl restart nginx       ← necesario
```

---

## 4. aa-complain: modo aprendizaje

`aa-complain` pone un perfil en modo complain (permite todo, registra
violaciones):

### Sintaxis

```bash
# Por ruta del ejecutable
sudo aa-complain /usr/sbin/nginx

# Por ruta del archivo de perfil
sudo aa-complain /etc/apparmor.d/usr.sbin.nginx
```

### Cuándo usar complain

```
Casos de uso de complain
══════════════════════════

  1. DESARROLLO de perfiles nuevos
     → Crear perfil básico → complain → ejecutar servicio
     → Revisar logs → agregar reglas faltantes → enforce

  2. DIAGNÓSTICO de problemas
     → "¿Es AppArmor quien bloquea mi servicio?"
     → Poner en complain → si funciona → el perfil necesita ajuste

  3. ACTUALIZACIÓN de servicios
     → Servicio actualizado puede necesitar nuevos accesos
     → Complain temporalmente → revisar logs → actualizar perfil

  4. AUDITORÍA
     → Ver qué accede un servicio sin bloquearlo
     → Útil para entender el comportamiento del servicio
```

### Qué hace internamente

```
aa-complain — acciones internas
═════════════════════════════════

  sudo aa-complain /usr/sbin/nginx
       │
       ▼
  1. Crear symlink en force-complain/
     ln -s /etc/apparmor.d/usr.sbin.nginx \
           /etc/apparmor.d/force-complain/usr.sbin.nginx
       │
       ▼
  2. Recargar perfil (ahora lee el flag de force-complain)
     apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
       │
       ▼
  Setting /etc/apparmor.d/usr.sbin.nginx to complain mode.
```

### Leer los logs de complain

```bash
# Buscar violaciones que complain registró
# Con auditd:
sudo ausearch -m APPARMOR_ALLOWED -ts recent

# Con journalctl:
journalctl -k --since "10 minutes ago" | grep ALLOWED

# Ejemplo de salida:
# audit: type=1400 audit(...): apparmor="ALLOWED"
#   operation="open" profile="/usr/sbin/nginx"
#   name="/srv/custom-site/index.html"
#   requested_mask="r" denied_mask="r"
```

```
Interpretar logs de complain
═════════════════════════════

  apparmor="ALLOWED"          ← Se permitió (no bloqueó)
  profile="/usr/sbin/nginx"   ← Perfil que habría bloqueado
  name="/srv/custom-site/..." ← Recurso accedido
  requested_mask="r"          ← Permiso que faltaría en enforce

  Acción: agregar al perfil:
    /srv/custom-site/** r,
```

### Flujo complain → enforce

```
Flujo típico de desarrollo de perfiles
════════════════════════════════════════

  1. Crear perfil inicial (aa-genprof o manual)
       │
       ▼
  2. Poner en complain
     sudo aa-complain /usr/sbin/myservice
       │
       ▼
  3. Ejecutar el servicio con todas sus funcionalidades
     sudo systemctl start myservice
     (usar el servicio normalmente, probar todas las rutas)
       │
       ▼
  4. Revisar logs de ALLOWED
     sudo aa-logprof         ← herramienta interactiva
     (o manualmente con ausearch/journalctl)
       │
       ▼
  5. Agregar reglas faltantes al perfil
       │
       ▼
  6. Repetir 3-5 hasta que no haya más ALLOWED
       │
       ▼
  7. Pasar a enforce
     sudo aa-enforce /usr/sbin/myservice
       │
       ▼
  8. Monitorear DENIED en producción
```

---

## 5. aa-disable: deshabilitar perfiles

`aa-disable` descarga un perfil del kernel y previene que se cargue en el
siguiente arranque:

### Sintaxis

```bash
# Deshabilitar perfil
sudo aa-disable /usr/sbin/nginx

# Equivalente manual:
sudo ln -s /etc/apparmor.d/usr.sbin.nginx /etc/apparmor.d/disable/
sudo apparmor_parser -R /etc/apparmor.d/usr.sbin.nginx
```

### Qué hace internamente

```
aa-disable — acciones internas
════════════════════════════════

  sudo aa-disable /usr/sbin/nginx
       │
       ▼
  1. Crear symlink en disable/
     ln -s /etc/apparmor.d/usr.sbin.nginx \
           /etc/apparmor.d/disable/usr.sbin.nginx
       │
       ▼
  2. Descargar perfil del kernel
     apparmor_parser -R /etc/apparmor.d/usr.sbin.nginx
       │
       ▼
  Disabling /etc/apparmor.d/usr.sbin.nginx.

  Resultado:
    ✓ Perfil descargado del kernel (efecto inmediato)
    ✓ Symlink en disable/ previene carga en próximo arranque
    ✗ El archivo del perfil NO se elimina
    ✗ Los procesos ya corriendo NO se "desconfinan"
      (siguen con el perfil hasta reiniciar)
```

### Rehabilitar un perfil deshabilitado

```bash
# Opción 1: aa-enforce (lo rehabilita y pone en enforce)
sudo aa-enforce /usr/sbin/nginx
# Elimina symlink de disable/ y carga el perfil

# Opción 2: Manual
sudo rm /etc/apparmor.d/disable/usr.sbin.nginx
sudo apparmor_parser -a /etc/apparmor.d/usr.sbin.nginx
```

### Verificar perfiles deshabilitados

```bash
# Listar symlinks en disable/
ls -la /etc/apparmor.d/disable/

# O buscar en aa-status la sección:
#   "X processes are unconfined but have a profile defined"
```

### disable vs eliminar el archivo

```
¿Deshabilitar o eliminar?
══════════════════════════

  aa-disable:
    ✓ Perfil sigue en /etc/apparmor.d/ (se puede rehabilitar)
    ✓ Symlink en disable/ documenta la decisión
    ✓ Actualizaciones del paquete pueden actualizar el perfil
    → Usar cuando: temporalmente no quieres confinar el servicio

  rm /etc/apparmor.d/usr.sbin.nginx:
    ✗ Perfil eliminado permanentemente
    ✗ Si el paquete se actualiza, puede recrear el perfil
    → Usar cuando: nunca (mejor usar aa-disable)
```

---

## 6. Gestión masiva de perfiles

### Todos los perfiles de un directorio

```bash
# Poner TODOS los perfiles en enforce
sudo aa-enforce /etc/apparmor.d/*

# Poner TODOS en complain
sudo aa-complain /etc/apparmor.d/*

# Recargar TODOS los perfiles
sudo apparmor_parser -r /etc/apparmor.d/
```

### Filtrar por estado actual

```bash
# Encontrar todos los perfiles en complain y pasarlos a enforce
sudo aa-status --complaining | while read profile; do
    sudo aa-enforce "$profile"
done

# Alternativa: leer del directorio force-complain
for f in /etc/apparmor.d/force-complain/*; do
    [ -L "$f" ] && sudo aa-enforce "$(basename "$f")"
done
```

### Gestión con systemd

```bash
# Recargar todos los perfiles (equivale a apparmor_parser -r para todos)
sudo systemctl reload apparmor

# Detener AppArmor (descarga TODOS los perfiles del kernel)
sudo systemctl stop apparmor
# ¡CUIDADO! Esto deja todos los servicios sin confinamiento

# Arrancar AppArmor (carga todos los perfiles)
sudo systemctl start apparmor
```

```
systemctl vs aa-* — ámbito
════════════════════════════

  systemctl stop apparmor
    → Descarga TODOS los perfiles
    → Ningún servicio confinado
    → Equivale a deshabilitar AppArmor temporalmente

  systemctl reload apparmor
    → Recarga TODOS los perfiles desde disco
    → Útil después de editar múltiples perfiles

  aa-disable /usr/sbin/nginx
    → Descarga UN perfil específico
    → Los demás servicios siguen confinados
    → Mucho más seguro que stop
```

---

## 7. Recarga y actualización de perfiles

### Cuándo recargar

```
Situaciones que requieren recargar
════════════════════════════════════

  1. Editaste el archivo del perfil manualmente
     → apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx

  2. Agregaste reglas en local/
     → apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx

  3. Modificaste una abstraction usada por varios perfiles
     → systemctl reload apparmor (recarga todos)

  4. Actualizaste un paquete que trajo un perfil nuevo
     → systemctl reload apparmor

  NO requiere recarga:
    - aa-enforce / aa-complain (recargan automáticamente)
    - aa-disable (descarga automáticamente)
```

### El flag -r (replace) vs -a (add)

```
-a vs -r: cuándo usar cada uno
═══════════════════════════════

  apparmor_parser -a PERFIL
    → add: cargar perfil NUEVO
    → FALLA si el perfil ya está cargado en el kernel
    → Uso: primera carga de un perfil recién creado

  apparmor_parser -r PERFIL
    → replace: reemplazar perfil existente
    → FALLA si el perfil NO está cargado
    → Uso: actualizar un perfil que ya está activo

  Cuál usar si no estás seguro:
    apparmor_parser -r PERFIL 2>/dev/null || apparmor_parser -a PERFIL
    (intenta replace, si falla hace add)

  O más simple:
    aa-enforce PERFIL
    (maneja ambos casos automáticamente)
```

### Actualización de perfiles desde paquetes

```
Flujo de actualización de paquetes
════════════════════════════════════

  apt upgrade apparmor-profiles
       │
       ▼
  Archivos en /etc/apparmor.d/ actualizados
  (dpkg respeta los mecanismos de conffiles)
       │
       ├── Si NO modificaste el perfil → se actualiza limpiamente
       │
       ├── Si MODIFICASTE el perfil → dpkg pregunta qué hacer
       │   (por eso se usan archivos en local/ en vez de editar directamente)
       │
       └── Archivos en local/ NUNCA se tocan por el paquete
           (tus customizaciones se preservan)
       │
       ▼
  Necesitas recargar:
    sudo systemctl reload apparmor
```

---

## 8. Personalización con local/

### Por qué usar local/ en vez de editar el perfil

```
Editar perfil directamente vs usar local/
═══════════════════════════════════════════

  DIRECTAMENTE (no recomendado):
  ──────────────────────────────
  /etc/apparmor.d/usr.sbin.nginx
    ...
    /var/www/html/** r,
    /srv/my-custom-site/** r,     ← tu cambio
    ...

  Problema: cuando el paquete actualiza el perfil, dpkg
  detecta el conflicto y te pregunta. Si aceptas la nueva
  versión, pierdes tu cambio.


  CON local/ (recomendado):
  ─────────────────────────
  /etc/apparmor.d/usr.sbin.nginx  ← no tocas este archivo
    ...
    #include <local/usr.sbin.nginx>   ← ya incluye local/
    ...

  /etc/apparmor.d/local/usr.sbin.nginx  ← TU archivo
    /srv/my-custom-site/** r,

  Ventaja: actualizaciones del paquete no afectan tus reglas.
```

### Crear una customización local

```bash
# 1. Verificar que el perfil incluye local/
grep "local/" /etc/apparmor.d/usr.sbin.nginx
# #include <local/usr.sbin.nginx>    ← debe existir esta línea

# 2. Editar (o crear) el archivo local
sudo nano /etc/apparmor.d/local/usr.sbin.nginx
```

```
# /etc/apparmor.d/local/usr.sbin.nginx
# Custom rules for nginx on this server

# Allow reading our custom web root
/srv/company-website/** r,

# Allow writing to custom log directory
/var/log/nginx-custom/*.log w,

# Allow reading SSL certs from custom path
/opt/certs/** r,
```

```bash
# 3. Verificar sintaxis
sudo apparmor_parser -p /etc/apparmor.d/usr.sbin.nginx

# 4. Recargar
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx

# 5. Reiniciar el servicio para que el proceso use las nuevas reglas
sudo systemctl restart nginx
```

### Ejemplo: permitir a MySQL usar un datadir personalizado

```bash
# MySQL movido a /data/mysql/ en vez de /var/lib/mysql/

# Verificar qué bloquea AppArmor
journalctl -k --since "5 minutes ago" | grep DENIED | grep mysql
# apparmor="DENIED" operation="open" profile="/usr/sbin/mysqld"
#   name="/data/mysql/ibdata1" requested_mask="rw"

# Agregar regla en local/
cat >> /etc/apparmor.d/local/usr.sbin.mysqld << 'EOF'
# Custom datadir
/data/mysql/ r,
/data/mysql/** rwk,
EOF

# Recargar y reiniciar
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.mysqld
sudo systemctl restart mysqld
```

---

## 9. Errores comunes

### Error 1: usar apparmor_parser -a con un perfil ya cargado

```bash
# Si el perfil ya está en el kernel:
sudo apparmor_parser -a /etc/apparmor.d/usr.sbin.nginx
# AppArmor parser error: Profile already exists in kernel

# Solución: usar -r (replace) en vez de -a (add)
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx

# O simplemente usar aa-enforce que maneja ambos casos:
sudo aa-enforce /usr/sbin/nginx
```

### Error 2: no reiniciar el servicio tras rehabilitar un perfil

```bash
# Perfil estaba deshabilitado, nginx corriendo sin confinar
sudo aa-enforce /usr/sbin/nginx
# Setting ... to enforce mode.

# ¿nginx ya está confinado?
sudo aa-status | grep nginx
# Puede aparecer en "unconfined but have profile defined"

# El proceso que ya corría NO adoptó el perfil automáticamente
# Solución:
sudo systemctl restart nginx

# Ahora sí:
sudo aa-status | grep nginx
# /usr/sbin/nginx (PID) → enforce
```

### Error 3: editar el perfil directamente en vez de usar local/

```bash
# Si editas /etc/apparmor.d/usr.sbin.nginx directamente,
# la próxima actualización del paquete puede sobrescribir tus cambios.

# Verificar si ya tienes cambios directos que deberías mover a local/
dpkg -V apparmor-profiles 2>/dev/null | grep apparmor.d
# Si muestra archivos modificados → mover cambios a local/
```

### Error 4: olvidar recargar después de editar

```bash
# Editaste /etc/apparmor.d/local/usr.sbin.nginx
# Pero no recargaste

# El kernel sigue usando la versión anterior
# Las reglas nuevas NO se aplican

# Siempre después de editar:
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx

# Si editaste varios perfiles o una abstraction:
sudo systemctl reload apparmor
```

### Error 5: confundir aa-disable con eliminar el perfil del disco

```bash
# aa-disable NO elimina el archivo
# Solo crea symlink en disable/ y descarga del kernel

ls /etc/apparmor.d/usr.sbin.nginx       # ← sigue existiendo
ls /etc/apparmor.d/disable/usr.sbin.nginx # ← symlink creado

# Para rehabilitar:
sudo aa-enforce /usr/sbin/nginx
# (elimina symlink de disable/ y carga en enforce)

# NUNCA hagas: rm /etc/apparmor.d/usr.sbin.nginx
# El paquete puede recrear el archivo en la próxima actualización
# y entonces tendrías un perfil cargado sin ser consciente de ello
```

---

## 10. Cheatsheet

```
╔══════════════════════════════════════════════════════════════════╗
║           Gestión de perfiles AppArmor — Cheatsheet            ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║  CAMBIAR MODO                                                    ║
║  sudo aa-enforce  /usr/sbin/X   Enforce (bloquea + registra)    ║
║  sudo aa-complain /usr/sbin/X   Complain (registra sin bloquear)║
║  sudo aa-disable  /usr/sbin/X   Deshabilitar (descargar perfil) ║
║                                                                  ║
║  APPARMOR_PARSER (bajo nivel)                                    ║
║  apparmor_parser -a PERFIL      Cargar perfil nuevo              ║
║  apparmor_parser -r PERFIL      Recargar perfil existente        ║
║  apparmor_parser -R PERFIL      Descargar perfil del kernel      ║
║  apparmor_parser -p PERFIL      Verificar sintaxis (dry run)     ║
║                                                                  ║
║  GESTIÓN GLOBAL                                                  ║
║  systemctl reload apparmor      Recargar todos los perfiles      ║
║  systemctl stop apparmor        Descargar TODOS (¡peligroso!)    ║
║  systemctl start apparmor       Cargar todos al arranque         ║
║                                                                  ║
║  DIRECTORIOS CLAVE                                               ║
║  /etc/apparmor.d/               Perfiles                         ║
║  /etc/apparmor.d/local/         Customizaciones (usar este)      ║
║  /etc/apparmor.d/disable/       Symlinks → perfil deshabilitado  ║
║  /etc/apparmor.d/force-complain/ Symlinks → forzar complain     ║
║                                                                  ║
║  VERIFICACIÓN                                                    ║
║  sudo aa-status                 Estado de todos los perfiles     ║
║  cat /sys/kernel/security/apparmor/profiles  Bajo nivel          ║
║                                                                  ║
║  FLUJO TRAS EDITAR UN PERFIL                                     ║
║  1. Editar /etc/apparmor.d/local/usr.sbin.X                     ║
║  2. apparmor_parser -p /etc/apparmor.d/usr.sbin.X  (verificar)  ║
║  3. apparmor_parser -r /etc/apparmor.d/usr.sbin.X  (recargar)   ║
║  4. systemctl restart X                            (aplicar)    ║
║                                                                  ║
║  RECORDAR                                                        ║
║  enforce↔complain: afecta procesos ya corriendo                  ║
║  disable→enforce: requiere reiniciar el servicio                 ║
║  Editar reglas: requiere recargar (-r) + reiniciar servicio      ║
║  Usar local/ para customizaciones, NUNCA editar perfil original  ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: Cambiar modo de un perfil

**Objetivo**: practicar la transición entre modos enforce, complain y disable.

```bash
# Paso 1: Ver el estado actual de todos los perfiles
sudo aa-status

# Paso 2: Elegir un perfil en enforce (ejemplo: tcpdump)
# Paso 3: Pasar a complain
sudo aa-complain /usr/sbin/tcpdump
```

> **Pregunta de predicción**: después de ejecutar `aa-complain`, ¿esperas
> encontrar un symlink nuevo en algún directorio de `/etc/apparmor.d/`? ¿En
> cuál?
>
> **Respuesta**: sí, habrá un nuevo symlink en
> `/etc/apparmor.d/force-complain/usr.sbin.tcpdump` apuntando al perfil
> original. Este symlink es lo que le dice a `apparmor_parser` que el perfil
> debe cargarse en modo complain.

```bash
# Paso 4: Verificar el symlink
ls -la /etc/apparmor.d/force-complain/

# Paso 5: Volver a enforce
sudo aa-enforce /usr/sbin/tcpdump

# Paso 6: Verificar que el symlink se eliminó
ls -la /etc/apparmor.d/force-complain/
```

### Ejercicio 2: Agregar regla via local/

**Escenario**: nginx necesita servir archivos desde `/opt/webapp/static/`.

```bash
# Paso 1: Verificar que nginx tiene perfil y está en enforce
sudo aa-status | grep nginx

# Paso 2: Intentar acceder a /opt/webapp/static/ (simulación)
# nginx devolvería 403 Forbidden

# Paso 3: Verificar el AVC
journalctl -k --since "5 minutes ago" | grep DENIED | grep nginx
```

> **Pregunta de predicción**: ¿qué valor tendrá `requested_mask` en el log
> de DENIED para una operación de lectura de archivos estáticos? ¿Qué regla
> exacta agregarías al archivo local/?
>
> **Respuesta**: `requested_mask="r"` (lectura). La regla sería:
> `/opt/webapp/static/** r,` en `/etc/apparmor.d/local/usr.sbin.nginx`.
> No olvides la coma al final de la regla — es obligatoria en la sintaxis de
> AppArmor.

```bash
# Paso 4: Agregar la regla
echo '/opt/webapp/static/** r,' | sudo tee -a /etc/apparmor.d/local/usr.sbin.nginx

# Paso 5: Verificar, recargar, reiniciar
sudo apparmor_parser -p /etc/apparmor.d/usr.sbin.nginx
sudo apparmor_parser -r /etc/apparmor.d/usr.sbin.nginx
sudo systemctl restart nginx
```

### Ejercicio 3: Diagnóstico con complain

**Escenario**: un servicio recién instalado no funciona correctamente. Usas
complain para descubrir qué accesos necesita.

```bash
# Paso 1: Poner el perfil en complain
sudo aa-complain /usr/sbin/cups-browsed

# Paso 2: Usar el servicio normalmente
sudo systemctl restart cups-browsed
# (imprimir algo, navegar impresoras)

# Paso 3: Revisar qué accesos hizo que el perfil no permitía
journalctl -k --since "5 minutes ago" | grep ALLOWED | grep cups
```

> **Pregunta de predicción**: si ves tres entradas ALLOWED con
> `name="/run/cups/cups.sock"`, `name="/etc/cups/cupsd.conf"` y
> `name="/var/cache/cups/job.cache"`, ¿a cuáles les darías permiso `r` y a
> cuál `rw`? ¿Por qué?
>
> **Respuesta**: `/etc/cups/cupsd.conf` → `r` (solo lectura, es configuración).
> `/run/cups/cups.sock` → `rw` (es un socket Unix, necesita leer y escribir
> para comunicarse). `/var/cache/cups/job.cache` → `rw` (es caché, necesita
> leer y escribir). Siempre aplicar **mínimo privilegio**: dar solo los
> permisos que el servicio realmente necesita.

---

> **Siguiente tema**: T03 — Crear perfiles (aa-genprof, aa-logprof, sintaxis
> de perfiles).
