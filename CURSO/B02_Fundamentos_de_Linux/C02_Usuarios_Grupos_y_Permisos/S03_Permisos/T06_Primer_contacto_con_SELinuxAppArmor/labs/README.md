# Lab — Primer contacto con SELinux/AppArmor

## Objetivo

Primer contacto con MAC (Mandatory Access Control): verificar el
estado de SELinux en AlmaLinux y AppArmor en Debian, inspeccionar
contextos y perfiles, y entender cuando estos sistemas pueden
bloquear operaciones.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — DAC vs MAC

### Objetivo

Entender por que existe MAC como complemento de los permisos Unix.

### Paso 1.1: Que son DAC y MAC

```bash
docker compose exec debian-dev bash -c '
echo "=== DAC (Discretionary Access Control) ==="
echo "- Permisos Unix tradicionales (rwx, ACLs)"
echo "- El propietario decide quien accede"
echo "- Si un proceso es comprometido, tiene todos los permisos del usuario"

echo ""
echo "=== MAC (Mandatory Access Control) ==="
echo "- Segunda capa, independiente de DAC"
echo "- El sistema (politica) decide que puede hacer cada programa"
echo "- Incluso root puede ser bloqueado"
echo "- SELinux (RHEL) o AppArmor (Debian)"

echo ""
echo "=== Ambos deben permitir el acceso ==="
echo "DAC permite? + MAC permite? → acceso concedido"
echo "Cualquiera bloquea → acceso denegado"
'
```

MAC agrega una capa de seguridad que los permisos Unix no pueden
proporcionar: limitar que puede hacer **cada programa**, no solo
cada usuario.

### Paso 1.2: Por que importa

```bash
docker compose exec debian-dev bash -c '
echo "=== Escenario sin MAC ==="
echo "1. nginx ejecuta como www-data"
echo "2. Vulnerabilidad en nginx permite ejecucion de codigo"
echo "3. El atacante tiene TODOS los permisos de www-data"
echo "4. Puede leer cualquier archivo que www-data pueda leer"

echo ""
echo "=== Escenario con MAC ==="
echo "1. nginx ejecuta como www-data"
echo "2. Vulnerabilidad en nginx permite ejecucion de codigo"
echo "3. MAC limita nginx a SOLO sus archivos especificos"
echo "4. El atacante no puede leer /etc/shadow ni otros archivos"
echo "   aunque los permisos Unix lo permitirian"
'
```

---

## Parte 2 — SELinux (AlmaLinux)

### Objetivo

Verificar el estado de SELinux y explorar contextos de seguridad.

### Paso 2.1: Estado de SELinux

```bash
docker compose exec alma-dev bash -c '
echo "=== getenforce ==="
getenforce 2>/dev/null || echo "(SELinux no disponible en este contenedor)"

echo ""
echo "=== sestatus ==="
sestatus 2>/dev/null || echo "(sestatus no disponible)"
'
```

Nota: dentro de contenedores Docker, SELinux puede no estar activo
porque el contenedor comparte el kernel del host. Los modos son:
Enforcing (bloquea), Permissive (solo registra), Disabled.

### Paso 2.2: Contextos de archivos

```bash
docker compose exec alma-dev bash -c '
echo "=== Contextos de archivos con -Z ==="
ls -Z /etc/passwd 2>/dev/null || echo "(SELinux no disponible)"
ls -Z /etc/shadow 2>/dev/null
ls -Z /usr/bin/passwd 2>/dev/null

echo ""
echo "Formato: user:role:type:level"
echo "El campo type es el mas importante para politicas"
'
```

SELinux asigna un contexto `user:role:type:level` a cada archivo.
Las politicas controlan que tipos de procesos pueden acceder a que
tipos de archivos.

### Paso 2.3: Contextos de procesos

```bash
docker compose exec alma-dev bash -c '
echo "=== Contextos de procesos ==="
ps -eZ 2>/dev/null | head -10 || echo "(SELinux no disponible)"

echo ""
echo "=== Contexto de tu shell ==="
id -Z 2>/dev/null || echo "(id -Z no disponible)"
'
```

### Paso 2.4: Herramientas de SELinux

```bash
docker compose exec alma-dev bash -c '
echo "=== Herramientas disponibles ==="
echo "getenforce — ver modo actual"
echo "setenforce — cambiar modo (temporal)"
echo "sestatus — estado detallado"
echo "ls -Z — ver contextos de archivos"
echo "ps -Z — ver contextos de procesos"
echo "id -Z — ver contexto de tu sesion"
echo "ausearch — buscar bloqueos en audit log"
echo "audit2why — explicar por que se bloqueo"

echo ""
echo "=== Disponibilidad ==="
which getenforce 2>/dev/null && echo "getenforce: disponible" || echo "getenforce: no disponible"
which sestatus 2>/dev/null && echo "sestatus: disponible" || echo "sestatus: no disponible"
which ausearch 2>/dev/null && echo "ausearch: disponible" || echo "ausearch: no disponible"
'
```

### Paso 2.5: Cambiar modo (si esta disponible)

```bash
docker compose exec alma-dev bash -c '
if command -v getenforce &>/dev/null && [ "$(getenforce 2>/dev/null)" != "Disabled" ]; then
    echo "=== Modo actual ==="
    getenforce
    echo ""
    echo "=== Cambiar a Permissive (temporal, para diagnostico) ==="
    setenforce 0 2>&1 || echo "(no se puede cambiar en contenedor)"
    getenforce
    echo ""
    echo "=== Restaurar a Enforcing ==="
    setenforce 1 2>&1 || echo "(no se puede cambiar en contenedor)"
else
    echo "SELinux no esta disponible o esta deshabilitado en este contenedor"
    echo "(normal en Docker — SELinux opera a nivel de host)"
fi
'
```

En contenedores Docker, SELinux generalmente no esta activo porque
el kernel del host controla SELinux, no el contenedor.

---

## Parte 3 — AppArmor (Debian)

### Objetivo

Verificar el estado de AppArmor y explorar perfiles.

### Paso 3.1: Estado de AppArmor

```bash
docker compose exec debian-dev bash -c '
echo "=== aa-status ==="
aa-status 2>/dev/null || echo "(AppArmor no disponible en este contenedor)"

echo ""
echo "=== Kernel module cargado? ==="
cat /sys/module/apparmor/parameters/enabled 2>/dev/null || echo "(no disponible)"
'
```

AppArmor puede no estar disponible dentro de contenedores Docker.
En un sistema Debian completo, estaria activo por defecto.

### Paso 3.2: Perfiles de AppArmor

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorio de perfiles ==="
ls /etc/apparmor.d/ 2>/dev/null | head -10 || echo "(directorio no existe en contenedor)"

echo ""
echo "=== Perfiles por programa ==="
echo "AppArmor define perfiles POR PROGRAMA (no por archivo como SELinux)"
echo "Cada perfil especifica que archivos y recursos puede usar un programa"
echo ""
echo "Ejemplo de perfil:"
echo "/usr/sbin/cupsd {"
echo "  #include <abstractions/base>"
echo "  capability net_bind_service,"
echo "  /etc/cups/** r,"
echo "  /var/spool/cups/** rw,"
echo "}"
'
```

### Paso 3.3: Modos de AppArmor

```bash
docker compose exec debian-dev bash -c '
echo "=== Modos de perfiles ==="
echo "enforce  — bloquea y registra (produccion)"
echo "complain — solo registra (diagnostico)"
echo "disabled — perfil no cargado"

echo ""
echo "=== Herramientas ==="
echo "aa-status  — ver estado y perfiles"
echo "aa-enforce — poner perfil en enforce"
echo "aa-complain — poner perfil en complain"
echo "aa-disable — desactivar perfil"
echo ""
echo "=== Disponibilidad ==="
which aa-status 2>/dev/null && echo "aa-status: disponible" || echo "aa-status: no disponible"
which aa-enforce 2>/dev/null && echo "aa-enforce: disponible" || echo "aa-enforce: no disponible"
'
```

### Paso 3.4: Comparacion rapida

```bash
docker compose exec debian-dev bash -c '
echo "=== SELinux vs AppArmor ==="
printf "%-20s %-25s %-25s\n" "Aspecto" "SELinux" "AppArmor"
printf "%-20s %-25s %-25s\n" "----------" "----------" "----------"
printf "%-20s %-25s %-25s\n" "Distribucion" "RHEL, Fedora, CentOS" "Debian, Ubuntu, SUSE"
printf "%-20s %-25s %-25s\n" "Modelo" "Etiquetas en todo" "Perfiles por programa"
printf "%-20s %-25s %-25s\n" "Complejidad" "Mayor" "Menor"
printf "%-20s %-25s %-25s\n" "Ver estado" "getenforce, sestatus" "aa-status"
printf "%-20s %-25s %-25s\n" "Ver contextos" "ls -Z, ps -Z" "aa-status"
printf "%-20s %-25s %-25s\n" "Configuracion" "/etc/selinux/" "/etc/apparmor.d/"
'
```

### Paso 3.5: Lo que necesitas saber ahora

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen practico ==="
echo ""
echo "1. MAC EXISTE — si algo falla misteriosamente, puede ser SELinux/AppArmor"
echo "2. COMO VERIFICAR:"
echo "   RHEL:   getenforce"
echo "   Debian: aa-status"
echo "3. COMO DIAGNOSTICAR:"
echo "   RHEL:   ausearch -m AVC --start recent"
echo "   Debian: grep -i apparmor /var/log/syslog"
echo "4. NO DESACTIVAR como primera opcion:"
echo "   setenforce 0 / aa-complain son para diagnostico, no produccion"
echo ""
echo "La configuracion completa se cubre en B11 (Seguridad)."
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. **MAC** (SELinux/AppArmor) es una segunda capa de permisos
   independiente de los permisos Unix (DAC). Ambos deben permitir
   el acceso.

2. SELinux usa **contextos** (user:role:type:level) en archivos y
   procesos. AppArmor usa **perfiles por programa**.

3. En contenedores Docker, SELinux/AppArmor pueden no estar activos
   porque operan a nivel de kernel del host.

4. Si algo falla misteriosamente en permisos: verificar con
   `getenforce` (RHEL) o `aa-status` (Debian) antes de desactivar.

5. `setenforce 0` y `aa-complain` son para **diagnostico**, no para
   produccion. La configuracion completa se cubre en B11.
