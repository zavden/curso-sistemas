# T02 — Archivos de unidad

> **Fuentes:** `README.md` + `README.max.md` + `labs/README.md`

## Erratas corregidas

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:16,100 | Diagrama: `Wants=nginx.service` dentro de nginx.service | Una unidad no puede "querer" a sí misma; eliminado |
| 2 | max:19-21 | `Type=forking` con `nginx -g daemon off;` | Contradictorio: si daemon está off, nginx corre en foreground → usar `Type=notify` o `Type=simple`; además falta comillas: `-g "daemon off;"` |
| 3 | max:207-211 | Variables `$CONTEXT_NAME`, `$LOGNAMESTAMP` listadas como disponibles | No existen; `$SERVICE_RESULT` solo está disponible en `ExecStopPost=`; las variables reales son `$MAINPID` y los especificadores (`%n`, `%i`, `%p`, etc.) |
| 4 | max:116 | `ConditionKernelCommandLine=boostetwork` | Typo → debería ser algo como `debug` o `systemd.debug` |
| 5 | max:221 | `Restart=on-abnormal` descrito como "por señal (SIGTERM, etc.)" | SIGTERM es la señal normal de stop; `on-abnormal` cubre señales inesperadas, timeout o watchdog, NO la terminación por SIGTERM |
| 6 | md:265 | `sudo cat > /etc/systemd/system/...service.d/10-limits.conf << 'EOF'` | `sudo cat > file` no funciona: la redirección `>` la ejecuta el shell actual (sin sudo); usar `sudo tee` o `sudo bash -c 'cat > ...'` |

---

## Ubicaciones y precedencia

Los archivos de unidad pueden existir en tres directorios, cada uno con un
propósito y una precedencia diferente:

```
Precedencia (alta → baja):

/etc/systemd/system/         ← ADMINISTRADOR (máxima)
    • Unidades creadas por ti
    • Overrides de unidades de paquetes
    • NO se sobrescribe con actualizaciones del paquete

/run/systemd/system/         ← RUNTIME (media)
    • Generadas durante el boot
    • Se pierden al reiniciar
    • Para configuraciones temporales

/usr/lib/systemd/system/     ← PAQUETES (mínima)
    • Instaladas por apt/dnf
    • Se sobrescriben con actualizaciones
    • En Debian: /lib/ puede ser symlink a /usr/lib/ (usr merge)
```

```bash
# Regla: si existe el mismo archivo en /etc/ y /usr/lib/, se usa el de /etc/
# El administrador SIEMPRE gana

# Ver de dónde viene una unidad:
systemctl show nginx.service --property=FragmentPath
# FragmentPath=/usr/lib/systemd/system/nginx.service

# Si hay un override en /etc/:
# FragmentPath=/etc/systemd/system/nginx.service

# Ver la configuración completa (original + overrides):
systemctl cat nginx.service
```

### Diferencia Debian vs RHEL

```bash
# Debian/Ubuntu también usa:
/lib/systemd/system/
# En sistemas con /usr merge: symlink a /usr/lib/systemd/system/
# En sistemas sin merge: directorio real

ls -la /lib/systemd/system    # Debian
# → /usr/lib/systemd/system  (con usr merge)

# RHEL siempre usa /usr/lib/systemd/system/
```

---

## Estructura de un archivo de unidad

Un archivo de unidad usa formato INI con secciones:

```ini
[Unit]
# Metadata y dependencias — común a TODOS los tipos de unidad

[Service]    # o [Socket], [Timer], [Mount], etc.
# Configuración específica del tipo

[Install]
# Cómo se integra al boot (enable/disable)
```

---

## Sección [Unit] — Metadata y dependencias

```ini
[Unit]
# Descripción legible (visible en systemctl status):
Description=My Custom Application Server

# Documentación (URL o man pages):
Documentation=https://example.com/docs
Documentation=man:myapp(8)

# Dependencias de orden (ver S02 para detalle):
After=network.target postgresql.service
# After= define ORDEN, no dependencia — si postgresql no está habilitado,
# no se activa automáticamente

# Dependencias de activación:
Requires=postgresql.service     # dependencia fuerte (falla si no arranca)
Wants=redis.service             # dependencia débil (intenta pero no falla)

# Conflictos:
Conflicts=sendmail.service      # no puede coexistir
```

### Directivas de condición (Condition*)

Las directivas `Condition*` verifican algo antes de activar la unidad. Si la
condición falla, la unidad no se activa (sin error, se marca "skipped"):

```ini
ConditionPathExists=/etc/myapp/config.conf    # el archivo existe
ConditionFileNotEmpty=/etc/myapp/config.conf  # el archivo tiene contenido
ConditionDirectoryNotEmpty=/var/log/myapp     # directorio no vacío
ConditionUser=root                             # solo si el usuario es root
ConditionArchitecture=x86_64                   # solo en esta arquitectura
ConditionVirtualization=!container             # NO en un contenedor
ConditionHost=server1.example.com             # solo en este host
ConditionKernelCommandLine=debug              # si el kernel tiene este parámetro
```

### Assert* — Como Condition, pero falla ruidosamente

```ini
# Condition: silently skips (la unidad no arranca, sin error)
# Assert: marca la unidad como "failed" con mensaje de error
AssertPathExists=/usr/local/bin/myapp
AssertUser=root
```

---

## Sección [Service] — Configuración del servicio

```ini
[Service]
# TIPO — cómo systemd determina que el servicio arrancó:
Type=simple              # (default) ExecStart es el proceso principal

# COMANDOS:
ExecStartPre=-/usr/local/bin/myapp-check   # antes de arrancar
#            ↑ el prefijo - permite que falle sin abortar
ExecStart=/usr/local/bin/myapp --config /etc/myapp/config.conf
ExecStartPost=/usr/local/bin/myapp-notify   # después de arrancar
ExecReload=/bin/kill -HUP $MAINPID          # al hacer reload
ExecStop=/usr/local/bin/myapp --stop        # al hacer stop (default: SIGTERM)

# USUARIO Y GRUPO:
User=myapp
Group=myapp

# DIRECTORIO DE TRABAJO:
WorkingDirectory=/var/lib/myapp

# VARIABLES DE ENTORNO:
Environment=NODE_ENV=production
Environment=PORT=3000
EnvironmentFile=/etc/myapp/env      # leer variables de un archivo

# REINICIO AUTOMÁTICO:
Restart=on-failure
RestartSec=5

# LÍMITES DE RECURSOS:
LimitNOFILE=65535
MemoryMax=512M
CPUQuota=80%
```

> **Nota:** `ExecStart=` requiere una **ruta absoluta**. Rutas relativas
> causan error en `systemd-analyze verify`.

### Tipos de servicio (Type=)

```
Type=simple (default):
  → ExecStart es el proceso principal
  → systemd considera el servicio "arrancado" cuando ExecStart inicia
  → Si ExecStart termina, el servicio termina

Type=exec:
  → Como simple, pero systemd espera a que execve() tenga éxito
  → El servicio es "started" solo si el binario se ejecuta exitosamente

Type=forking:
  → ExecStart hace fork() y el proceso padre termina
  → systemd espera a que el padre termine y trackea al hijo
  → Usado para daemons estilo SysV (PIDFile= suele ser necesario)

Type=oneshot:
  → ExecStart corre y TERMINA (ej: un script de setup)
  → RemainAfterExit=yes para que systemd lo considere "active" después
  → Puede tener múltiples ExecStart= (se ejecutan secuencialmente)

Type=notify:
  → Como simple, pero el servicio envía sd_notify() cuando está listo
  → systemd espera la notificación antes de marcarlo como active
  → Más preciso que simple (sabe cuándo realmente está listo)

Type=dbus:
  → El servicio adquiere un BusName en D-Bus
  → systemd espera a que el BusName esté disponible
```

### Variables disponibles en directivas

```bash
$MAINPID          # PID del proceso principal
$SERVICE_RESULT   # Resultado del servicio (solo en ExecStopPost=)

# Especificadores (se expanden al cargar la unidad):
%n                # nombre completo de la unidad (nginx.service)
%p                # prefijo del nombre (nginx)
%i                # nombre de instancia (para unidades template)
%N                # nombre de la unidad sin escape
%t                # directorio de runtime (/run para root, /run/user/UID)
%h                # home del usuario
```

### Directivas de reinicio

```ini
# Restart: cuándo reiniciar automáticamente
Restart=no                 # nunca (default)
Restart=on-success         # si sale con código 0
Restart=on-failure         # si sale con código != 0, señal, o timeout
Restart=on-abnormal        # por señal inesperada, timeout o watchdog
                           # (NO por exit code != 0, NO por SIGTERM normal)
Restart=on-abort           # solo si muere por señal no capturada
Restart=on-watchdog        # solo si watchdog no recibe ping
Restart=always             # siempre (excepto systemctl stop)

# Tiempo entre reintentos:
RestartSec=5               # segundos antes de reiniciar

# Límites de reintento:
StartLimitIntervalSec=10   # ventana de tiempo
StartLimitBurst=3          # máximo de reinicios en esa ventana
# Si 3 reinicios ocurren en 10 segundos, el servicio falla definitivamente
```

---

## Sección [Install] — Enable y disable

```ini
[Install]
# WantedBy — el target que "quiere" esta unidad (el más común):
WantedBy=multi-user.target
# enable crea un symlink en /etc/systemd/system/multi-user.target.wants/

# RequiredBy — dependencia fuerte (el target falla si esta unidad falla):
RequiredBy=graphical.target

# Also — unidades adicionales a habilitar/deshabilitar juntas:
Also=myapp.socket myapp-worker.service

# Alias — nombres alternativos:
Alias=webapp.service
```

```bash
# Lo que hace systemctl enable:
systemctl enable nginx.service
# 1. Lee [Install] → WantedBy=multi-user.target
# 2. Crea symlink:
#    /etc/systemd/system/multi-user.target.wants/nginx.service
#    → /usr/lib/systemd/system/nginx.service
# 3. El servicio arrancará cuando multi-user.target se active

# Lo que hace systemctl disable:
systemctl disable nginx.service
# Elimina el symlink — el servicio ya NO arranca automáticamente

# Ver los symlinks:
ls -la /etc/systemd/system/multi-user.target.wants/
```

### Unidades "static"

Las unidades sin sección `[Install]` son "static": no se pueden
`enable`/`disable` directamente. Solo se activan como dependencia de otra
unidad.

```bash
systemctl is-enabled systemd-journald.service
# static
```

---

## systemctl edit — Overrides y drop-ins

El método correcto para modificar una unidad es `systemctl edit`, no editar
el archivo directamente:

### Drop-in (override parcial)

```bash
# Crea un archivo de override en:
# /etc/systemd/system/nginx.service.d/override.conf
sudo systemctl edit nginx.service
# Abre un editor con un archivo vacío
```

```ini
# Solo pones las directivas que quieres cambiar:
[Service]
MemoryMax=1G
Environment=WORKER_PROCESSES=4
Restart=always
```

```bash
# La unidad resultante combina:
# 1. /usr/lib/systemd/system/nginx.service  (original del paquete)
# 2. /etc/systemd/system/nginx.service.d/override.conf  (tu override)

# Ver la configuración efectiva (merged):
systemctl cat nginx.service
# Muestra el archivo original + overrides, cada uno con su ruta
```

### Drop-ins múltiples

```bash
# Se pueden crear múltiples drop-ins (se aplican en orden alfabético):
# /etc/systemd/system/nginx.service.d/
#   10-limits.conf
#   20-restart.conf
#   30-logging.conf

# Crear manualmente (usando tee para que sudo funcione con redirección):
sudo mkdir -p /etc/systemd/system/nginx.service.d/
echo '[Service]
LimitNOFILE=65535' | sudo tee /etc/systemd/system/nginx.service.d/10-limits.conf

sudo systemctl daemon-reload
```

### Limpiar directivas de lista antes de redefinir

```ini
# Algunas directivas (como ExecStart) se ACUMULAN en drop-ins.
# Para reemplazar en vez de acumular, asignar vacío primero:
[Service]
ExecStart=                            # limpia el ExecStart original
ExecStart=/usr/local/bin/myapp-v2     # nuevo valor
```

### Override completo

```bash
# Copia el archivo COMPLETO a /etc/ (pierde updates del paquete):
sudo systemctl edit --full nginx.service
# Copia /usr/lib/systemd/system/nginx.service a /etc/systemd/system/
# y abre el editor

# CUIDADO: las actualizaciones del paquete NO se aplicarán
# mientras tengas el override en /etc/
```

### Revertir cambios

```bash
# Eliminar un drop-in:
sudo rm -rf /etc/systemd/system/nginx.service.d/
sudo systemctl daemon-reload

# Revertir un override completo:
sudo rm /etc/systemd/system/nginx.service
sudo systemctl daemon-reload

# systemctl revert (systemd 235+) — elimina todos los overrides:
sudo systemctl revert nginx.service
```

---

## daemon-reload

Cada vez que se modifica un archivo de unidad, hay que notificar a systemd:

```bash
sudo systemctl daemon-reload
# Re-lee todos los archivos de unidad
# NO reinicia servicios — solo recarga la configuración en memoria

# Después, reiniciar el servicio para que tome los cambios:
sudo systemctl restart nginx.service
```

```bash
# Si olvidas daemon-reload, systemctl te avisa:
# Warning: nginx.service changed on disk. Run 'systemctl daemon-reload'

# systemctl edit hace daemon-reload automáticamente al guardar
```

---

## systemctl mask — Desactivar permanentemente

```bash
# mask: symlink a /dev/null — imposible de activar
sudo systemctl mask nginx.service
# Crea: /etc/systemd/system/nginx.service → /dev/null
# Ni systemctl start puede ejecutarlo

# unmask: quitar el mask
sudo systemctl unmask nginx.service

# disable vs mask:
# disable → no arranca solo, pero se puede ejecutar con systemctl start
# mask → completamente bloqueado, ni start funciona
```

---

## systemd-delta — Ver diferencias

```bash
# Ver todas las unidades que fueron modificadas respecto al paquete:
systemd-delta

# Tipos de diferencia:
# [EXTENDED]    → tiene drop-ins
# [OVERRIDDEN]  → archivo en /etc/ reemplaza al de /usr/
# [MASKED]      → masked por /etc/ o /run/
# [EQUIVALENT]  → mismo contenido, diferentes timestamps

# Filtrar por tipo:
systemd-delta --type=extended
```

---

## Generators — Unidades dinámicas

Los generators son programas que generan unidades dinámicamente durante el
boot:

```bash
# Ubicaciones de generators:
/usr/lib/systemd/system-generators/    # paquetes
/etc/systemd/system-generators/        # admin
/run/systemd/system-generators/        # runtime

# Generators comunes:
systemd-fstab-generator       # /etc/fstab → .mount y .swap
systemd-cryptsetup-generator   # /etc/crypttab → unidades LUKS
systemd-gpt-auto-generator     # particiones GPT → .mount, .swap
systemd-getty-generator        # ttys → getty@.service

# Se ejecutan al boot y al hacer daemon-reload
# Su salida va a /run/systemd/generator/
ls /run/systemd/generator/
```

---

## Inspeccionar unidades

```bash
# Ver el archivo de unidad completo (con overrides):
systemctl cat sshd.service

# Ver todas las propiedades:
systemctl show sshd.service

# Ver propiedades específicas:
systemctl show sshd.service --property=MainPID,ActiveState,SubState,Type

# Ver qué archivo se usa:
systemctl show sshd.service --property=FragmentPath

# Ver drop-ins aplicados:
systemctl show sshd.service --property=DropInPaths

# Verificar sintaxis (sin aplicar):
systemd-analyze verify /etc/systemd/system/myapp.service

# Ver todas las unidades que difieren del paquete:
systemd-delta
```

---

## Lab — Archivos de unidad

### Parte 1 — Ubicaciones y precedencia

#### Paso 1.1: Los tres directorios

```bash
echo "--- /usr/lib/systemd/system/ (paquetes, precedencia MÍNIMA) ---"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l

echo "--- /etc/systemd/system/ (admin, precedencia MÁXIMA) ---"
ls /etc/systemd/system/*.service 2>/dev/null | wc -l

echo "--- /run/systemd/system/ (runtime, precedencia INTERMEDIA) ---"
ls /run/systemd/system/*.service 2>/dev/null | wc -l
```

#### Paso 1.2: Verificar overrides existentes

```bash
# Buscar servicios en /etc/ que sobreescriben a los de /usr/lib/:
for f in /etc/systemd/system/*.service 2>/dev/null; do
    [[ -f "$f" ]] || continue
    BASE=$(basename "$f")
    if [[ -f "/usr/lib/systemd/system/$BASE" ]]; then
        echo "  OVERRIDE: $BASE (/etc/ sobrescribe /usr/lib/)"
    fi
done
```

#### Paso 1.3: Debian vs RHEL

```bash
# Debian: /lib/ puede ser symlink a /usr/lib/ (usr merge)
if [[ -L /lib/systemd/system ]]; then
    echo "/lib/systemd/system → $(readlink /lib/systemd/system)"
elif [[ -d /lib/systemd/system ]]; then
    echo "/lib/systemd/system es directorio real"
fi

# RHEL siempre usa /usr/lib/systemd/system/
```

### Parte 2 — Estructura interna

#### Paso 2.1: Leer un servicio completo

```bash
SVC=$(ls /usr/lib/systemd/system/systemd-journald.service 2>/dev/null | head -1)
[[ -n "$SVC" ]] && cat -n "$SVC"

# Identificar las secciones:
grep -n "^\[" "$SVC"
```

Observa las tres secciones: `[Unit]`, `[Service]`, `[Install]`.

#### Paso 2.2: Explorar tipos de servicio en el sistema

```bash
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    TYPE=$(grep "^Type=" "$svc" 2>/dev/null | head -1)
    [[ -n "$TYPE" ]] && printf "  %-45s %s\n" "$(basename "$svc")" "$TYPE"
done | head -10
```

#### Paso 2.3: Unidades static (sin [Install])

```bash
COUNT=0
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    if ! grep -q "^\[Install\]" "$svc" 2>/dev/null; then
        echo "  $(basename "$svc") — static (sin [Install])"
        ((COUNT++))
        [[ $COUNT -ge 5 ]] && break
    fi
done
```

Las unidades static no se pueden `enable`/`disable` directamente — se activan
como dependencia de otra unidad.

### Parte 3 — Drop-ins y verificación

#### Paso 3.1: Crear un unit file de ejemplo

```bash
cat > /tmp/lab-example.service << 'EOF'
[Unit]
Description=Lab Example Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

systemd-analyze verify /tmp/lab-example.service 2>&1 || true
```

Si no hay errores, la unidad es sintácticamente correcta.

#### Paso 3.2: Detectar errores de sintaxis

```bash
cat > /tmp/lab-bad.service << 'EOF'
[Unit]
Description=Bad Service

[Service]
Type=simple
ExecStart=relative-path
Restart=maybe

[Install]
WantedBy=multi-user.target
EOF

systemd-analyze verify /tmp/lab-bad.service 2>&1 || true
```

`systemd-analyze verify` detecta: rutas relativas en `ExecStart=`, valores
inválidos para `Restart=`, directivas en la sección incorrecta.

#### Paso 3.3: Explorar drop-ins existentes

```bash
# Buscar drop-ins:
find /etc/systemd/system -name "*.d" -type d 2>/dev/null

# Ver diferencias respecto a los paquetes:
systemd-delta --type=extended 2>/dev/null | head -10
```

---

## Ejercicios

### Ejercicio 1 — Contar unidades por ubicación

```bash
echo "Paquetes:"; ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo "Admin:"; ls /etc/systemd/system/*.service 2>/dev/null | wc -l
echo "Runtime:"; ls /run/systemd/system/*.service 2>/dev/null | wc -l

find /etc/systemd/system -name "*.d" -type d 2>/dev/null
```

¿Cuántas unidades tiene cada directorio? ¿Hay drop-ins?

<details><summary>Predicción</summary>

- **Paquetes** (`/usr/lib/`): la gran mayoría (típicamente 100-200+ servicios)
- **Admin** (`/etc/`): pocas o ninguna (solo si el admin creó servicios custom)
- **Runtime** (`/run/`): pocas (generadas dinámicamente, temporales)

Los drop-ins (directorios `.d/`) aparecen si algún servicio fue modificado
con `systemctl edit`. En un sistema recién instalado, puede no haber ninguno.

La mayoría de unidades vienen de paquetes — el admin solo crea/modifica
las que necesita personalizar.

</details>

### Ejercicio 2 — Leer y analizar una unidad

```bash
systemctl cat sshd.service

# Identificar:
systemctl show sshd.service --property=FragmentPath
systemctl show sshd.service --property=DropInPaths
systemctl show sshd.service --property=Type,Restart,ExecStart
```

¿En qué directorio está `sshd.service`? ¿Tiene drop-ins? ¿Qué `Type=` usa?

<details><summary>Predicción</summary>

```
FragmentPath=/usr/lib/systemd/system/sshd.service
DropInPaths=              (vacío si no se ha modificado)
Type=notify               (o exec, según la distro)
Restart=on-failure
ExecStart=/usr/sbin/sshd -D $SSHD_OPTS (o similar)
```

`sshd` está en `/usr/lib/` (del paquete openssh-server). Sin drop-ins en
un sistema por defecto. `Type=notify` significa que sshd envía `sd_notify()`
a systemd cuando está listo. El `-D` mantiene sshd en foreground (necesario
para `Type=notify` o `Type=simple`).

`systemctl cat` muestra el archivo con una cabecera `# /ruta/al/archivo`
antes de cada fragmento.

</details>

### Ejercicio 3 — Verificar un unit file con errores

Crea estos dos archivos y verifica con `systemd-analyze verify`:

```bash
# Archivo bueno:
cat > /tmp/good.service << 'EOF'
[Unit]
Description=Good Service

[Service]
Type=oneshot
ExecStart=/usr/bin/echo "hello"
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

# Archivo con errores:
cat > /tmp/bad.service << 'EOF'
[Unit]
Description=Bad Service

[Service]
Type=simple
ExecStart=no-absolute-path
Restart=whenever-I-feel-like-it

[Install]
WantedBy=multi-user.target
EOF

systemd-analyze verify /tmp/good.service 2>&1
echo "---"
systemd-analyze verify /tmp/bad.service 2>&1
```

¿Qué errores detecta `systemd-analyze verify`?

<details><summary>Predicción</summary>

**good.service:** sin errores (o warnings menores sobre dependencias
no encontradas, que se ignoran en archivos aislados).

**bad.service:** al menos dos errores:
1. `ExecStart=no-absolute-path` — ruta no absoluta (debe empezar con `/`)
2. `Restart=whenever-I-feel-like-it` — valor inválido para `Restart=`
   (los valores válidos son: `no`, `on-success`, `on-failure`, `on-abnormal`,
   `on-abort`, `on-watchdog`, `always`)

`systemd-analyze verify` es estático — no necesita `daemon-reload` ni
cargar la unidad en systemd. Es el equivalente de `bash -n` + `shellcheck`
para unit files.

</details>

### Ejercicio 4 — Precedencia: /etc/ gana

```bash
# Ver dónde está sshd.service:
systemctl show sshd.service --property=FragmentPath

# Si copias a /etc/, ¿cuál toma precedencia?
# (NO ejecutar si sshd es crítico para tu acceso — solo razonar)
```

Si existiera `/etc/systemd/system/sshd.service` Y
`/usr/lib/systemd/system/sshd.service`, ¿cuál usaría systemd?

<details><summary>Predicción</summary>

Systemd usaría `/etc/systemd/system/sshd.service` (el de `/etc/`).

Precedencia: `/etc/` > `/run/` > `/usr/lib/`.

Esto permite al administrador personalizar cualquier servicio sin modificar
los archivos del paquete. Pero tiene un riesgo: si haces un override
completo (`systemctl edit --full`), las actualizaciones del paquete
**no se aplican** — tu copia en `/etc/` tiene prioridad.

Por eso se prefieren los **drop-ins** (`systemctl edit` sin `--full`):
solo sobreescriben las directivas específicas, y el resto se hereda del
archivo original del paquete (que sí recibe actualizaciones).

</details>

### Ejercicio 5 — ExecStart: limpiar antes de redefinir

Un drop-in con `ExecStart=` se **acumula** con el original. ¿Qué pasa si
escribes esto?

```ini
# /etc/systemd/system/sshd.service.d/override.conf
[Service]
ExecStart=/usr/sbin/sshd -D -p 2222
```

<details><summary>Predicción</summary>

**Error.** Systemd intentará ejecutar **ambos** `ExecStart`:
1. El original: `/usr/sbin/sshd -D $SSHD_OPTS`
2. El del drop-in: `/usr/sbin/sshd -D -p 2222`

Solo `Type=oneshot` permite múltiples `ExecStart=`. Para otros tipos,
tener dos `ExecStart=` causa un error.

La forma correcta es **limpiar** primero asignando vacío:

```ini
[Service]
ExecStart=
ExecStart=/usr/sbin/sshd -D -p 2222
```

La primera línea vacía el `ExecStart` del archivo original.
La segunda define el nuevo valor.

Esto solo aplica a directivas que se acumulan (como `ExecStart=`,
`Environment=`). Directivas escalares (como `Restart=`, `Type=`) se
sobreescriben directamente.

</details>

### Ejercicio 6 — systemctl mask vs disable

```bash
# Verificar estado de un servicio:
systemctl is-enabled cron.service 2>/dev/null || \
    systemctl is-enabled crond.service 2>/dev/null
```

¿Cuál es la diferencia entre `disable` y `mask`?

<details><summary>Predicción</summary>

| Acción | `disable` | `mask` |
|--------|-----------|--------|
| `systemctl start` | Funciona | **Falla** |
| Arranca al boot | No | No |
| Qué hace | Elimina symlink de `.wants/` | Crea symlink a `/dev/null` |
| `is-enabled` muestra | `disabled` | `masked` |
| Revertir | `enable` | `unmask` |

`disable` solo quita el arranque automático — el servicio se puede iniciar
manualmente con `systemctl start`.

`mask` bloquea completamente el servicio — ni siquiera `systemctl start`
puede ejecutarlo. Es útil para servicios que no quieres que NADA active
(ni manual ni como dependencia de otro servicio).

Ejemplo práctico: masquear `sendmail.service` si usas `postfix`, para que
ninguna dependencia lo reactive accidentalmente.

</details>

### Ejercicio 7 — Condition vs Assert

```bash
cat > /tmp/test-cond.service << 'EOF'
[Unit]
Description=Test Condition
ConditionPathExists=/tmp/test-flag

[Service]
Type=oneshot
ExecStart=/usr/bin/echo "Condition met"
RemainAfterExit=yes
EOF

systemd-analyze verify /tmp/test-cond.service 2>&1 || true
```

Si `/tmp/test-flag` NO existe, ¿qué pasa al intentar arrancar el servicio?
¿Y si usaras `AssertPathExists=` en lugar de `ConditionPathExists=`?

<details><summary>Predicción</summary>

**Con `ConditionPathExists`:** el servicio se marca como **"skipped"** —
no arranca, pero NO es un error. `systemctl status` muestra:
```
○ test-cond.service
   Loaded: loaded
   Active: inactive (dead)
   Condition: start condition unmet
```

**Con `AssertPathExists`:** el servicio se marca como **"failed"** — es
un error explícito. `systemctl status` muestra:
```
× test-cond.service
   Loaded: loaded
   Active: failed
   Assert: AssertPathExists=/tmp/test-flag failed
```

Usar `Condition*` para cosas opcionales (ej: "solo si estamos en un
contenedor"). Usar `Assert*` para requisitos obligatorios (ej: "el
binario DEBE existir").

</details>

### Ejercicio 8 — Explorar generators

```bash
# Ver generators instalados:
ls /usr/lib/systemd/system-generators/

# Ver unidades generadas:
ls /run/systemd/generator/ 2>/dev/null | head -10

# Comparar con /etc/fstab:
grep -v "^#" /etc/fstab | grep -v "^$"

# Ver un mount generado:
systemctl cat tmp.mount 2>/dev/null
```

¿De dónde viene `tmp.mount`? ¿Quién lo creó?

<details><summary>Predicción</summary>

`tmp.mount` fue generado por `systemd-fstab-generator` a partir de una
línea en `/etc/fstab` (si `/tmp` está ahí), o puede venir directamente
como un unit file en `/usr/lib/systemd/system/`.

`systemctl cat tmp.mount` muestra la ruta: si dice `/run/systemd/generator/`,
fue generado dinámicamente por `systemd-fstab-generator` durante el boot.
Si dice `/usr/lib/systemd/system/`, es un archivo estático del paquete.

Los generators se ejecutan al boot y al hacer `daemon-reload`. Convierten
configuraciones legacy (como `/etc/fstab` y `/etc/crypttab`) en unidades
systemd nativas.

</details>

### Ejercicio 9 — systemd-delta: encontrar modificaciones

```bash
systemd-delta 2>/dev/null
```

¿Qué significan los tipos `[EXTENDED]`, `[OVERRIDDEN]`, `[MASKED]`?

<details><summary>Predicción</summary>

- `[EXTENDED]` — la unidad tiene drop-ins que **extienden** la configuración
  original (archivos en `.d/`)
- `[OVERRIDDEN]` — un archivo en `/etc/` **reemplaza completamente** al
  de `/usr/lib/` (override completo con `systemctl edit --full`)
- `[MASKED]` — la unidad está masked (symlink a `/dev/null`)
- `[EQUIVALENT]` — archivos idénticos en distintas rutas (mismo contenido,
  diferente timestamp)

En un sistema recién instalado, puede no haber ninguna entrada (nada
modificado). En un sistema en producción, es común ver `[EXTENDED]` para
servicios que fueron tuneados con drop-ins.

`systemd-delta` es útil para auditar qué ha cambiado respecto a los
defaults del paquete — especialmente antes de actualizaciones del sistema.

</details>

### Ejercicio 10 — Anatomía completa de un servicio web

Lee este unit file y responde las preguntas:

```ini
[Unit]
Description=My Web Application
Documentation=https://docs.myapp.io
After=network-online.target postgresql.service
Requires=postgresql.service
Wants=redis.service
ConditionPathExists=/etc/myapp/config.yml

[Service]
Type=notify
User=myapp
Group=myapp
WorkingDirectory=/opt/myapp
EnvironmentFile=/etc/myapp/env
ExecStartPre=/opt/myapp/bin/check-config
ExecStart=/opt/myapp/bin/server --config /etc/myapp/config.yml
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=10
StartLimitIntervalSec=60
StartLimitBurst=3
MemoryMax=1G
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

1. ¿Qué pasa si postgresql no está corriendo?
2. ¿Qué pasa si redis no está corriendo?
3. ¿Qué pasa si `/etc/myapp/config.yml` no existe?
4. ¿Qué pasa si el servicio crashea 4 veces en 60 segundos?

<details><summary>Predicción</summary>

1. **PostgreSQL no corriendo:** el servicio **falla** al arrancar.
   `Requires=postgresql.service` es una dependencia fuerte — si postgresql
   no puede arrancar, myapp tampoco. `After=` asegura que postgresql
   arranque primero.

2. **Redis no corriendo:** el servicio **arranca igual**.
   `Wants=redis.service` es una dependencia débil — systemd intenta
   arrancar redis, pero si falla, myapp continúa sin problema.

3. **Config no existe:** el servicio se marca como **"skipped"** (no
   arranca, sin error). `ConditionPathExists=` hace una verificación
   silenciosa.

4. **4 crasheos en 60s:** el servicio entra en estado **"failed"**
   permanente. `StartLimitBurst=3` en `StartLimitIntervalSec=60` significa:
   máximo 3 reinicios en 60 segundos. El cuarto intento supera el límite
   y systemd deja de reiniciar. Para recuperarlo: `systemctl reset-failed`
   y luego `systemctl start`.

Otros detalles: `Type=notify` significa que myapp usa `sd_notify()` para
indicar cuándo está listo. `ExecStartPre=` valida la config antes de
arrancar (sin `-`, un fallo aborta el inicio). `$MAINPID` en `ExecReload=`
se expande al PID del proceso principal.

</details>
