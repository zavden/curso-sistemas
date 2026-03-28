# T02 — Archivos de unidad

## Anatomía de un archivo de unidad

Los archivos de unidad son archivos de texto plano en formato INI. Systemd los lee y convierte en unidades activas en memoria:

```
┌─────────────────────────────────────────────────────────────┐
│                  /etc/systemd/system/nginx.service          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  [Unit]                                                      │
│      Description=nginx HTTP Server                          │
│      Documentation=https://nginx.org/en/docs/               │
│      After=network.target                                    │
│      Wants=nginx.service                                    │
│                                                              │
│  [Service]                                                   │
│      Type=forking                                           │
│      PIDFile=/run/nginx.pid                                 │
│      ExecStart=/usr/sbin/nginx -g daemon off;              │
│      ExecReload=kill -HUP $MAINPID                          │
│      ExecStop=/bin/kill -WINCH $MAINPID                     │
│      Restart=always                                         │
│      RestartSec=5                                           │
│                                                              │
│  [Install]                                                   │
│      WantedBy=multi-user.target                              │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Secciones de un archivo de unidad

Un archivo de unidad siempre tiene tres secciones:

```
[Unit]       → Metadata y relaciones (común a TODOS los tipos de unidad)
[Service]    → Configuración del tipo específico (aquí: servicio)
[Install]    → Qué hacer cuando se hace enable/disable
```

## Ubicaciones y precedencia

```
┌─────────────────────────────────────────────────────────────┐
│                     PRECEDENCIA (alta → baja)                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  /etc/systemd/system/      ← ADMINISTRADOR (máxima)        │
│     • Unidades creadas por ti                              │
│     • Overrides de unidades de paquetes                    │
│     • NO se sobrescribe con actualizaciones del sistema     │
│                                                              │
│  /run/systemd/system/      ← RUNTIME (media)               │
│     • Generadas durante el boot                             │
│     • Se pierden al reiniciar                               │
│     • Para configuraciones temporales                       │
│                                                              │
│  /usr/lib/systemd/system/  ← PAQUETES (mínima)            │
│     • Instaladas por apt/dnf                               │
│     • Se sobrescriben con actualizaciones                   │
│     • /lib → symlink a /usr/lib en sistemas con /usr merge│
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

```bash
# Regla: si existe en /etc/ y en /usr/lib/, se usa el de /etc/
# El administrador SIEMPRE gana

# Ver de dónde viene una unidad:
systemctl show nginx.service --property=FragmentPath
# FragmentPath=/usr/lib/systemd/system/nginx.service

# Ver si tiene overrides:
systemctl show nginx.service --property=DropInPaths
# DropInPaths=/etc/systemd/system/nginx.service.d

# Ver el contenido completo (original + overrides):
systemctl cat nginx.service
```

## Sección [Unit] — Metadata y dependencias

```ini
[Unit]
# Descripción legible (visible en systemctl status):
Description=Nginx HTTP Server

# Documentación (man pages o URLs):
Documentation=https://nginx.org/en/docs/
Documentation=man:nginx(8)

# DEPENDENCIAS (ver S02 para detalle completo):
# Después de qué target/units deben estar activas
After=network.target remote-fs.target nss-lookup.target

# Dependencias inversas (qué units quieren esta):
Wants=nginx.service
```

### Directivas de condición (Condition*)

Las directivas `Condition*` verifican algo antes de activar la unidad. Si la condición falla, la unidad no se activa (sin error):

```ini
# Condiciones comunes:
ConditionPathExists=/etc/myapp/config.conf    # el archivo existe
ConditionFileNotEmpty=/etc/myapp/config.conf # el archivo tiene contenido
ConditionDirectoryNotEmpty=/var/log/myapp    # el directorio no está vacío
ConditionUser=root                             # solo si el usuario es root
ConditionArchitecture=x86_64                   # solo en esta arquitectura
ConditionVirtualization=!container              # NO en un contenedor
ConditionHost=server1.example.com              # solo en este host
ConditionKernelCommandLine=boostetwork         # si el kernel tiene boot param
```

```bash
# Si ConditionPathExists falla, la unidad se marca "skipped":
# systemctl status myapp
# ○ myapp.service
#    Loaded: loaded (...); Condition check resulted in
#    myapp.service being skipped.
```

### Assert* — Como Condition, pero falla ruidosamente

```ini
# AssertPathExists es como ConditionPathExists, pero:
# - Condition: silently skips
# - Assert: marca la unidad como "failed" con mensaje de error
AssertPathExists=/usr/local/bin/myapp
AssertUser=root
```

## Sección [Service] — Configuración del servicio

```ini
[Service]
# TIPO — cómo systemd detecta que el servicio arrancó
Type=simple              # (default) ExecStart es el proceso principal

# USER — como se ejecuta el servicio
User=myapp
Group=myapp

# DIRECTORIO DE TRABAJO
WorkingDirectory=/var/lib/myapp

# VARIABLES DE ENTORNO
Environment=NODE_ENV=production
Environment=PORT=3000
EnvironmentFile=/etc/myapp/env        # leer vars de un archivo

# COMANDOS
ExecStartPre=-/usr/local/bin/myapp-check   # - = puede fallar sin abortar
ExecStart=/usr/local/bin/myapp --config /etc/myapp/config.conf
ExecStartPost=/usr/local/bin/myapp-notify
ExecReload=/bin/kill -HUP $MAINPID          # señal para reload
ExecStop=/bin/kill -WINCH $MAINPID          # señal para stop (default: SIGTERM)
ExecStopPost=/bin/logger "myapp stopped"

# REINICIO AUTOMÁTICO
Restart=on-failure
RestartSec=5

# RECURSOS
MemoryMax=512M
CPUQuota=80%
LimitNOFILE=65535
```

### Tipos de Service

```
Type=simple (default):
  → ExecStart es el proceso principal
  → systemd considera el servicio "arrancado" cuando ExecStart inicia
  → Si ExecStart termina, el servicio termina

Type=forking:
  → ExecStart hace fork() y el proceso padre termina
  → systemd espera a que el proceso padre termine
  → El proceso hijo es el servicio
  → Usado para daemons estilo SysV (nginx, sshd, postgresql)

Type=oneshot:
  → ExecStart corre y TERMINA (ej: un script de setup)
  → WantedBy= puede apuntar a este servicio
  → RemainAfterExit=yes para que systemd lo considere "active" después

Type=notify:
  → Como simple, pero el servicio envía sd_notify() cuando está listo
  → systemd espera la notificación antes de marcarlo como active
  → Usado por: systemd itself, firewalld, nginx con -g "daemon off"

Type=dbus:
  → El servicio toma un BusName en D-Bus
  → systemd espera a que el BusName esté disponible
  → El servicio está "active" mientras el BusName exista
```

### Variables disponibles en directivas de servicio

```
$MAINPID          → PID del proceso principal (el que ExecStart arranca)
$CONTEXT_NAME     → Nombre del contexto
$SERVICE_RESULT   → Resultado del servicio (success, failure, etc.)
$TMPDIR           → Directorio temporal del servicio
$LOGNAMESTAMP    → Timestamp del log
```

### Directivas de reinicio

```ini
# Restart: cuándo reiniciar
Restart=no                 # nunca
Restart=on-success         # si sale con código 0
Restart=on-failure         # si sale con código != 0 o por señal
Restart=on-abnormal         # por señal (SIGTERM, etc.)
Restart=on-abort           # si el proceso muere sin clean exit
Restart=on-watchdog        # si watchdog no recibe ping

# RestartSec: segundos entre intento de restart y siguiente
RestartSec=5

# StartLimitInterval/StartLimitBurst: límites de restart
# Si se intentan más de Burst restarts en Interval segundos, el servicio falla
StartLimitIntervalSec=10s
StartLimitBurst=3
```

## Sección [Install] — Enable y disable

```ini
[Install]
# TARGET que quiere esta unidad (el más común):
WantedBy=multi-user.target
# Al hacer enable: crea symlink en:
# /etc/systemd/system/multi-user.target.wants/myapp.service
# → /usr/lib/systemd/system/myapp.service

# RequiredBy: igual pero más estricto (fallo si el target no puede):
RequiredBy=graphical.target

# ALSO: unidades que se habilitan/deshabilitan junto con esta:
Also=myapp.timer myapp-healthcheck.service

# Alias: nombres alternativos:
Alias=webapp.service
```

```bash
# systemctl enable nginx.service
# 1. Busca [Install] → WantedBy=multi-user.target
# 2. Crea symlink:
#    /etc/systemd/system/multi-user.target.wants/nginx.service
#    → /usr/lib/systemd/system/nginx.service
# 3. El servicio arrancará cuando multi-user.target se active

# systemctl disable nginx.service
# Elimina el symlink
# El servicio YA NO arranca automáticamente

# Ver qué targets "quieren" una unidad:
systemctl show nginx.service --property=WantedBy,RequiredBy
```

## systemctl edit — Overrides y drop-ins

El método correcto para modificar una unidad es usar `systemctl edit`, no editar el archivo directo:

### Drop-in (override parcial)

```bash
# Crea un archivo de override en:
# /etc/systemd/system/nginx.service.d/override.conf
sudo systemctl edit nginx.service
```

```ini
# Solo pones las directivas que quieres cambiar:
[Service]
# Override: más memoria
MemoryMax=1G

# Override: más workers
Environment=WORKER_PROCESSES=8

# Override: siempre reiniciar
Restart=always
```

```bash
# Lo que pasa:
# 1. systemctl crea /etc/systemd/system/nginx.service.d/
# 2. Guarda override.conf ahí
# 3. Al recargar, systemd combina:
#    /usr/lib/systemd/system/nginx.service (ORIGINAL)
#    + /etc/systemd/system/nginx.service.d/override.conf (OVERRIDE)
#    → Configuración EFECTIVA
```

### Ver la configuración combinada

```bash
# systemctl cat MUESTRA todo (original + overrides):
systemctl cat nginx.service

# Salida:
## /usr/lib/systemd/system/nginx.service
# [Unit]
# Description=nginx HTTP Server
# ...
# [Service]
# Type=notify
# ExecStart=/usr/sbin/nginx -g daemon off;
# ...
#
## /etc/systemd/system/nginx.service.d/override.conf
# [Service]
# MemoryMax=1G
# Environment=WORKER_PROCESSES=8
```

### Drop-ins múltiples

```bash
# Se pueden tener múltiples drop-ins (se aplican en orden alfabético):
# /etc/systemd/system/nginx.service.d/
#   10-memory.conf
#   20-workers.conf
#   30-logging.conf

# Útil para organizar overrides por tema:
# - 10-limits.conf → Resource limits
# - 20-environment.conf → Environment variables
# - 30-restart.conf → Restart policies
```

### Override completo

```bash
# Copia el archivo COMPLETO a /etc/ (pierde updates del paquete):
sudo systemctl edit --full nginx.service
# Copia /usr/lib/systemd/system/nginx.service
# a /etc/systemd/system/nginx.service
# Abre el editor

# ⚠️ CUIDADO: las actualizaciones del paquete NO se aplicarán
# mientras tengas el override en /etc/
```

### Revertir cambios

```bash
# Opción 1: eliminar drop-ins manualmente
sudo rm -rf /etc/systemd/system/nginx.service.d/
sudo systemctl daemon-reload

# Opción 2: usar systemctl revert (systemd 235+)
sudo systemctl revert nginx.service
# Elimina todos los overrides y drop-ins
# Restaura la versión del paquete

# Opción 3: systemctl edit y dejar vacío
# (funciona porque un drop-in vacío no cambia nada)
```

## daemon-reload — Recargar configuración

```bash
# Después de CREAR, MODIFICAR o ELIMINAR un archivo de unidad:
sudo systemctl daemon-reload

# daemon-reload:
# ✓ Re-escanea /usr/lib/, /etc/, /run/
# ✓ Recarga los archivos de unidad en memoria
# ✗ NO reinicia servicios activos
# ✗ NO recarga servicios (para eso: systemctl reload)
```

```bash
# Señales de que necesitas daemon-reload:
# 1. "Warning: nginx.service changed on disk. Run 'systemctl daemon-reload'"
# 2. Creaste/modificaste un archivo .service en /etc/systemd/system/
# 3. Modificaste con systemctl edit (el editor hace daemon-reload automático)

# Workflow completo después de modificar una unidad:
sudo systemctl edit nginx.service
# → Se guarda el drop-in
# → systemctl hace daemon-reload automático
# → Ahora hay que reiniciar para que tome los cambios:
sudo systemctl restart nginx.service
```

## systemd-delta — Ver diferencias

```bash
# Ver todas las unidades que fueron modificadas:
systemd-delta

# Tipos de diferencia:
# [EXTENDED]  → tiene drop-ins
# [OVERRIDDEN] → archivo en /etc/ reemplaza al de /usr/
# [MASKED]    → masked por /etc/ o /run/
# [EQUIVALENT]→ mismo contenido, diferentes timestamps
```

```bash
# Ver diferencia específica:
systemd-delta --type=extended /etc/systemd/system/nginx.service

# Equivalente a:
systemctl cat nginx.service          # ver efectivo
systemctl show nginx.service         # ver propiedades
```

## systemctl mask — Desactivar permanentemente

```bash
# mask: symlink a /dev/null — imposible de activar
sudo systemctl mask nginx.service
# Crea: /etc/systemd/system/nginx.service → /dev/null
# Ni systemctl start nginx puede ejecutarlo

# unmask: quitar el mask
sudo systemctl unmask nginx.service

# Para desactivar un servicio SIN mask:
sudo systemctl disable nginx.service
# → El servicio no arranca solo, pero puede ejecutarse manualmente
```

## Generators — Unidades dinámicas

Los generators son programas ejecutables que generan unidades durante el boot:

```bash
# Ubicaciones de generators:
/usr/lib/systemd/system-generators/    # paquetes
/etc/systemd/system-generators/         # admin
/run/systemd/system-generators/         # runtime

# Generators built-in:
systemd-fstab-generator       # /etc/fstab → .mount
systemd-gpt-auto-generator    # particiones GPT → .mount, .swap
systemd-cryptsetup-generator   # /etc/crypttab → .device
systemd-getty-generator       # ttys → getty@.service
```

```bash
# Los generators escriben a:
ls /run/systemd/generator/

# Se ejecutan:
# - Al boot
# - Al hacer daemon-reload
# - Output es units temporales en /run/systemd/system/
```

## Verificación de unidades

```bash
# Verificar sintaxis (sin aplicar):
systemd-analyze verify /etc/systemd/system/myapp.service
# Reporta errores de sintaxis, referencias a units que no existen, etc.

# Ver el estado de una unidad:
systemctl status nginx.service

# Ver propiedades específicas:
systemctl show nginx.service --property=MainPID,ActiveState,SubState,Type
systemctl show nginx.service --property=FragmentPath,DropInPaths,UnitFileState

# Ver qué necesita/en que orden:
systemctl list-dependencies nginx.service
systemctl list-dependencies --reverse nginx.service
```

---

## Ejercicios

### Ejercicio 1 — Explorar ubicaciones de unidades

```bash
# 1. Contar unidades en cada ubicación
echo "Paquetes:"; ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo "Admin:"; ls /etc/systemd/system/*.service 2>/dev/null | wc -l
echo "Runtime:"; ls /run/systemd/system/*.service 2>/dev/null | wc -l

# 2. Ver drop-ins
find /etc/systemd/system -name "*.d" -type d

# 3. Ver unidades con drop-ins activos
systemd-delta --type=extended 2>/dev/null | head -20

# 4. Ver la ruta efectiva de sshd
systemctl show sshd.service --property=FragmentPath,DropInPaths
```

### Ejercicio 2 — Leer y analizar una unidad completa

```bash
# Elegir un servicio activo (ej: cron o sshd)
# 1. Ver el archivo efectivo (original + overrides)
systemctl cat sshd.service

# 2. Identificar:
# - ¿En qué directorio está el archivo original?
# - ¿Tiene drop-ins?
# - ¿Qué Type= usa?
# - ¿Qué After= tiene?
# - ¿Qué WantedBy= tiene?

# 3. Ver propiedades
systemctl show sshd.service --property=Type,User,Group,ExecStart,Restart

# 4. ¿Qué target lo quiere?
systemctl show sshd.service --property=WantedBy
```

### Ejercicio 3 — Crear un override

```bash
# 1. Crear un drop-in de prueba para un servicio no crítico
# (sshd es crítico — usa nginx o cron para esto)

# 2. Crear override
sudo systemctl edit cron.service
# Agregar:
# [Service]
# Environment="CRON_TZ=America/New_York"
# RestartSec=10

# 3. Ver qué se creó
ls -la /etc/systemd/system/cron.service.d/
cat /etc/systemd/system/cron.service.d/*.conf

# 4. Ver el archivo combinado
systemctl cat cron.service | tail -10

# 5. Recargar y ver que está bien
sudo systemctl daemon-reload
systemctl status cron.service

# 6. Limpiar (eliminar el override)
sudo rm -rf /etc/systemd/system/cron.service.d/
sudo systemctl daemon-reload
```

### Ejercicio 4 — systemctl mask vs disable

```bash
# 1. Ver estado actual de un servicio
systemctl is-enabled nginx.service
systemctl is-active nginx.service

# 2. Disable
sudo systemctl disable nginx.service
systemctl is-enabled nginx.service
# Ver el symlink
ls -la /etc/systemd/system/multi-user.target.wants/ | grep nginx

# 3. Enable
sudo systemctl enable nginx.service

# 4. Mask (probar con algo que puedas romper)
# sudo systemctl mask nginx.service
# systemctl is-enabled nginx.service  # masked
# sudo systemctl start nginx.service  # fallará

# 5. Unmask
# sudo systemctl unmask nginx.service
```

### Ejercicio 5 — Condiciones y asserts

```bash
# 1. Crear un servicio de prueba que use ConditionPathExists
sudo cat > /tmp/test-condition.service << 'EOF'
[Unit]
Description=Test Condition
ConditionPathExists=/tmp/test-file-exists
After=network.target

[Service]
Type=oneshot
ExecStart=/usr/bin/logger "Test service ran"
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

# 2. Copiar a /etc/
sudo cp /tmp/test-condition.service /etc/systemd/system/

# 3. Sin el archivo -> debería ser skipped
sudo systemctl daemon-reload
sudo systemctl start test-condition.service
systemctl status test-condition.service

# 4. Crear el archivo y volver a intentar
sudo touch /tmp/test-file-exists
sudo systemctl start test-condition.service
systemctl status test-condition.service

# 5. Limpiar
sudo rm /etc/systemd/system/test-condition.service
sudo rm /tmp/test-condition.service /tmp/test-file-exists
sudo systemctl daemon-reload
```

### Ejercicio 6 — Investigar generators

```bash
# 1. Ver qué genera fstab-generator
systemctl list-units --type=mount | head -10
# home.mount, tmp.mount, etc. — generados desde /etc/fstab

# 2. Ver lasmounts que vienen de fstab
grep -v "^#" /etc/fstab | grep -v "^$"

# 3. Ver la unidad mount generada para /tmp
systemctl cat tmp.mount

# 4. Ver si hay generators en /usr/lib
ls /usr/lib/systemd/system-generators/

# 5. Los generators escriben a /run/systemd/generator/
ls /run/systemd/generator/ 2>/dev/null | head -10
```
