# T02 — Crear una unidad de servicio custom

## Erratas corregidas respecto a las fuentes originales

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:63,68 | "esta unidad ARRANCAn" — typo en conjugación | "esta unidad ARRANCA" (singular) |
| 2 | max:107 | Conflicts: "myapp NO puede arrancar" si apache2 está corriendo | Incorrecto: Conflicts **detiene** apache2 al arrancar myapp; no impide el arranque |
| 3 | max:201 | `Type=notify` con `nginx -g "daemon on;"` | Contradictorio: `daemon on;` hace fork (patrón forking), incompatible con notify. Para Type=notify usar `daemon off;` |
| 4 | max:245-249 | Variables `$CONTEXT_NAME`, `$LOGO` como disponibles en Exec* | Fabricadas: no existen en systemd. Las reales son `$MAINPID`, `$SERVICE_RESULT`, `$EXIT_CODE`, `$EXIT_STATUS` |
| 5 | max:322 | `Restart=on-start-limit-hit` como valor válido de Restart= | No existe. Los valores válidos son: `no`, `on-success`, `on-failure`, `on-abnormal`, `on-abort`, `on-watchdog`, `always` |
| 6 | max:568 | `systemctl dot` para generar grafo de dependencias | El comando correcto es `systemd-analyze dot` |
| 7 | max:599 | Propiedad `ExecMainExitCode` | No existe; la propiedad correcta es `ExecMainCode` |

---

## Anatomía completa de un .service

```
┌─────────────────────────────────────────────────────────────┐
│               /etc/systemd/system/myapp.service             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  [Unit]                    ← metadata y relaciones          │
│      Description=My Application Server                      │
│      After=network.target postgresql.service                │
│      Wants=postgresql.service                               │
│      ConditionPathExists=/etc/myapp/config.yaml             │
│                                                             │
│  [Service]                 ← configuración del servicio     │
│      Type=notify                                            │
│      User=myapp                                             │
│      Group=myapp                                            │
│      WorkingDirectory=/opt/myapp                            │
│      ExecStart=/opt/myapp/bin/server                        │
│      ExecReload=/bin/kill -HUP $MAINPID                     │
│      Restart=on-failure                                     │
│      RestartSec=10                                          │
│      EnvironmentFile=/etc/myapp/env                         │
│      StateDirectory=myapp                                   │
│      RuntimeDirectory=myapp                                 │
│                                                             │
│  [Install]                 ← qué hacer al enable/disable   │
│      WantedBy=multi-user.target                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Las tres secciones:
- **[Unit]** — Metadata y relaciones (común a todos los tipos de unidad)
- **[Service]** — Configuración específica del servicio
- **[Install]** — Cómo se integra al boot

## Sección [Unit] — Detalle completo

### Description y Documentation

```ini
[Unit]
# Descripción legible (aparece en systemctl status y logs):
Description=My Application Server

# Documentación (URLs, man pages, archivos):
Documentation=https://example.com/docs
Documentation=man:myapp(8)
Documentation=file:///usr/share/doc/myapp/README.md
# Múltiples valores separados por espacios o líneas adicionales
```

### Dependencias de orden (After/Before)

```ini
[Unit]
# After: esta unidad ARRANCA después de las listadas
After=network.target postgresql.service remote-fs.target
# → postgresql se arranca primero (si está enabled)
# → Esta unidad espera a que postgresql esté active

# Before: esta unidad ARRANCA antes de las listadas
Before=nginx.service
# → Esta unidad arranca ANTES de nginx

# IMPORTANTE: After/Before solo definen ORDEN, no activan nada
# Si postgresql no está enabled, esta unidad arranca igual
# Para crear una DEPENDENCIA real, usar Wants/Requires
```

### Dependencias de activación (Wants/Requires)

```ini
[Unit]
# Wants: desea activar las unidades listadas (dependencia débil)
# SI fallan, esta unidad ARRANCA IGUAL
Wants=postgresql.service redis.service
# → Intenta arrancar postgresql y redis
# → Pero si uno falla, myapp arranca igual

# Requires: requiere las unidades listadas (dependencia fuerte)
# SI alguna falla, esta unidad FALLA
Requires=postgresql.service
# → postgresql debe arrancar OK
# → Si postgresql falla, myapp también falla

# Combinar orden + activación:
Wants=postgresql.service
After=postgresql.service
# → Intenta arrancar postgresql
# → Espera a que esté activo antes de arrancar myapp
# → myapp arranca aunque postgresql falle (Wants, no Requires)
```

### Conflicts — Exclusión mutua

```ini
[Unit]
# Conflicts: no puede coexistir con estas unidades
Conflicts=apache2.service
# → Al arrancar myapp, apache2 se detiene automáticamente
# → Al arrancar apache2, myapp se detiene automáticamente
```

### Condiciones y Assertions

```ini
[Unit]
# Conditions: si fallan, la unidad se marca "skipped" silenciosamente
ConditionPathExists=/etc/myapp/config.yaml
ConditionFileNotEmpty=/etc/myapp/config.yaml
ConditionDirectoryNotEmpty=/var/log/myapp
ConditionVirtualization=!container          # NO en contenedor
ConditionArchitecture=x86_64
ConditionHost=server1.example.com

# Assertions: si fallan, la unidad se marca "failed" con error claro
AssertPathExists=/opt/myapp/bin/server
```

## Sección [Service] — Detalle completo

### Type — Cómo systemd detecta que el servicio arrancó

```
┌──────────────────────────────────────────────────────────────┐
│  Type=simple (default)                                       │
│  → ExecStart es el proceso principal                         │
│  → systemctl start retorna cuando ExecStart inicia           │
│  → Si ExecStart termina → servicio dead                      │
│  → USAR para servicios que corren continuamente              │
├──────────────────────────────────────────────────────────────┤
│  Type=exec                                                   │
│  → Como simple pero espera a que exec() tenga éxito          │
│  → Detecta binario no encontrado, permisos, etc.             │
│  → Disponible en systemd 240+                                │
├──────────────────────────────────────────────────────────────┤
│  Type=forking                                                │
│  → ExecStart hace fork() y el padre termina                  │
│  → systemd espera a que el padre termine                     │
│  → Necesita PIDFile para saber cuál es el hijo (el daemon)   │
│  → EVITAR — es el modelo legacy SysV                         │
├──────────────────────────────────────────────────────────────┤
│  Type=notify                                                 │
│  → El servicio envía sd_notify(READY=1) cuando está listo    │
│  → systemctl start espera esa notificación                   │
│  → El servicio debe usar libsystemd / sd_notify()            │
├──────────────────────────────────────────────────────────────┤
│  Type=oneshot                                                │
│  → ExecStart CORRE Y TERMINA                                 │
│  → RemainAfterExit=yes → queda "active (exited)" después     │
│  → Útil para scripts de setup/inicialización                 │
│  → Puede tener múltiples ExecStart= (se ejecutan en orden)   │
├──────────────────────────────────────────────────────────────┤
│  Type=dbus                                                   │
│  → El servicio toma un BusName en D-Bus                      │
│  → systemctl start espera hasta que el BusName exista        │
│  → Útil para servicios que ofrecen D-Bus APIs                │
├──────────────────────────────────────────────────────────────┤
│  Type=idle                                                   │
│  → Como simple, pero espera a que todos los jobs terminen    │
│  → Retrasa la ejecución hasta que no hay jobs pendientes     │
│  → Útil para evitar mezclar salida con mensajes de boot      │
└──────────────────────────────────────────────────────────────┘
```

```ini
# Ejemplo: simple (para aplicaciones que corren en foreground)
[Service]
Type=simple
ExecStart=/opt/myapp/bin/server
# systemd considera "active" cuando ExecStart se ejecuta

# Ejemplo: notify (el servicio reporta cuándo está listo)
[Service]
Type=notify
ExecStart=/opt/myapp/bin/server
# systemd espera sd_notify(READY=1) desde el proceso
# El servicio debe estar compilado con soporte sd_notify

# Ejemplo: oneshot (script de inicialización)
[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/setup-database
RemainAfterExit=yes
# systemd marca "active (exited)" cuando ExecStart termina
```

### Comandos de ejecución (Exec*)

```ini
[Service]
# Comando principal — DEBE ser ruta ABSOLUTA (no busca en PATH):
ExecStart=/opt/myapp/bin/server --config /etc/myapp/config.yaml

# Antes de arrancar (se ejecutan en orden; si uno falla, el servicio no arranca):
ExecStartPre=/opt/myapp/bin/check-config
ExecStartPre=/usr/bin/mkdir -p /run/myapp

# Después de arrancar:
ExecStartPost=/opt/myapp/bin/notify-ready

# Al hacer reload (systemctl reload):
ExecReload=/bin/kill -HUP $MAINPID
# $MAINPID es el PID del proceso principal
# O: ExecReload=/opt/myapp/bin/server --reload

# Al detener (si no está definido: SIGTERM → espera TimeoutStopSec → SIGKILL):
ExecStop=/opt/myapp/bin/server --graceful-shutdown

# Después de detenerse (se ejecuta siempre, incluso si falló):
ExecStopPost=/bin/rm -f /run/myapp/socket
```

### Prefijos especiales en Exec*

```ini
# - = ignorar el exit code (no marcar como failed si falla):
ExecStartPre=-/opt/myapp/bin/check-optional-dependency

# + = ejecutar con privilegios completos (ignora User/Group):
ExecStartPre=+/usr/bin/setcap cap_net_bind_service=+ep /opt/myapp/bin/server

# ! = ejecutar sin ambient capabilities elevadas:
ExecStart=!/opt/myapp/bin/server

# : = no aplicar las restricciones de seguridad del servicio:
ExecStartPre=:/opt/myapp/bin/privileged-check
```

### Variables especiales disponibles en Exec*

```
$MAINPID         → PID del proceso principal (el de ExecStart)
$SERVICE_RESULT  → Resultado del servicio (solo en ExecStop/ExecStopPost)
$EXIT_CODE       → Código de salida (solo en ExecStopPost)
$EXIT_STATUS     → Estado de salida numérico (solo en ExecStopPost)

Especificadores (se expanden en cualquier directiva):
%n  → nombre completo de la unidad (myapp.service)
%p  → prefijo del nombre (myapp)
%i  → instancia (para templates: myapp@instance)
%t  → directorio runtime (/run para system, $XDG_RUNTIME_DIR para user)
%S  → directorio de estado (/var/lib para system)
%h  → home del usuario
```

### Usuario, grupo y directorios gestionados

```ini
[Service]
User=myapp
Group=myapp

# Crear usuario dinámico (systemd 235+):
DynamicUser=yes
# → Crea usuario/grupo efímero para la ejecución
# → Se elimina cuando el servicio para
# → Las rutas de datos deben usar StateDirectory/CacheDirectory

# Directorios gestionados automáticamente (con permisos correctos):
StateDirectory=myapp           # /var/lib/myapp (datos persistentes)
CacheDirectory=myapp           # /var/cache/myapp
LogsDirectory=myapp            # /var/log/myapp
ConfigurationDirectory=myapp   # /etc/myapp (read-only para el servicio)
RuntimeDirectory=myapp         # /run/myapp (se elimina al parar)

# Beneficios:
# - Se crean con ownership del User= configurado
# - No necesitas ExecStartPre=mkdir
# - Funciona con DynamicUser=yes
# - RuntimeDirectory se limpia automáticamente
```

### Variables de entorno

```ini
[Service]
# Variables directas:
Environment=NODE_ENV=production
Environment="PORT=3000" "HOST=0.0.0.0"
# Las comillas permiten espacios en el valor

# Desde un archivo (formato KEY=VALUE por línea):
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local
# El prefijo - lo hace opcional (no falla si no existe)

# Pasar variables del entorno del manager:
PassEnvironment=LANG TZ
```

```bash
# Formato del archivo de entorno (/etc/myapp/env):
DATABASE_URL=postgresql://localhost/myapp
SECRET_KEY=abc123
LOG_LEVEL=info
# SIN export, SIN comillas (a menos que sean parte del valor)
# Líneas con # son comentarios
```

### Directorio de trabajo y salida

```ini
[Service]
WorkingDirectory=/opt/myapp
# Directorio de trabajo del proceso

StandardOutput=journal
StandardError=journal
# Enviar stdout/stderr al journal (es el default)
# Alternativas: null, file:/path, append:/path, socket

SyslogIdentifier=myapp
# Nombre que aparece en los logs del journal

UMask=0077
# umask del proceso (default: 0022)
```

### Restart y Watchdog

```ini
[Service]
# Restart: cuándo reiniciar automáticamente
Restart=no               # nunca (default)
Restart=on-success       # solo si terminó con código 0
Restart=on-failure       # si terminó con código != 0, señal, timeout, o watchdog
Restart=on-abnormal      # si terminó por señal, timeout, o watchdog
Restart=on-abort         # si terminó por señal no manejada (SIGABRT, SIGSEGV)
Restart=on-watchdog      # solo si watchdog no recibió ping
Restart=always           # siempre (excepto systemctl stop)

RestartSec=5             # segundos entre reinicio

# Watchdog: el servicio debe reportar que sigue vivo periódicamente
WatchdogSec=30           # si no reporta en 30s, se reinicia
# El servicio debe llamar sd_notify("WATCHDOG=1") periódicamente
```

### Recursos y límites

```ini
[Service]
# Memoria:
MemoryMax=512M              # máximo absoluto
MemoryHigh=384M             # umbral de advertencia (throttle)

# CPU:
CPUQuota=50%                # porcentaje de 1 core (200% = 2 cores)

# Archivos abiertos:
LimitNOFILE=65535

# Procesos:
LimitNPROC=1024

# Core dumps:
LimitCORE=infinity
```

## Sección [Install] — Detalle completo

```ini
[Install]
# WantedBy: el target que "quiere" esta unidad (dependencia débil)
WantedBy=multi-user.target
# Al enable: crea symlink en:
# /etc/systemd/system/multi-user.target.wants/myapp.service

# RequiredBy: dependencia fuerte (si falla, el target falla)
RequiredBy=graphical.target

# Also: unidades que se habilitan/deshabilitan junto con esta
Also=myapp.timer myapp-healthcheck.service

# Alias: nombres alternativos
Alias=webapp.service
# systemctl start webapp → ejecuta myapp
```

## Ejemplos completos

### Aplicación Node.js

```ini
[Unit]
Description=Node.js API Server
Documentation=https://api.example.com/docs
After=network.target postgresql.service redis.service
Wants=postgresql.service redis.service

[Service]
Type=simple
User=nodeapp
Group=nodeapp
WorkingDirectory=/opt/nodeapp
ExecStart=/usr/bin/node /opt/nodeapp/server.js
Restart=on-failure
RestartSec=10
Environment=NODE_ENV=production
EnvironmentFile=/etc/nodeapp/env
StandardOutput=journal
StandardError=journal
SyslogIdentifier=nodeapp
MemoryMax=1G
RuntimeDirectory=nodeapp
StateDirectory=nodeapp

[Install]
WantedBy=multi-user.target
```

### Script de inicialización (oneshot)

```ini
[Unit]
Description=Initialize Application Database
After=postgresql.service
Requires=postgresql.service

[Service]
Type=oneshot
User=postgres
Group=postgres
ExecStart=/opt/myapp/bin/init-db.sh
StandardOutput=journal
StandardError=journal
RemainAfterExit=yes

# No tiene [Install] — se habilita desde un timer o manualmente
```

### Servicio con pre-checks y hardening

```ini
[Unit]
Description=My Secure Application
After=network-online.target postgresql.service
Wants=network-online.target
StartLimitIntervalSec=600
StartLimitBurst=5

[Service]
Type=notify
User=myapp
Group=myapp
WorkingDirectory=/opt/myapp

# Pre-checks
ExecStartPre=/opt/myapp/bin/check-config /etc/myapp/config.yaml
ExecStartPre=/opt/myapp/bin/check-db-connection

# Main
ExecStart=/opt/myapp/bin/server
ExecReload=/bin/kill -HUP $MAINPID

# Restart
Restart=on-failure
RestartSec=10
WatchdogSec=60
TimeoutStartSec=60
TimeoutStopSec=30

# Environment
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local

# Directorios
StateDirectory=myapp
RuntimeDirectory=myapp

# Hardening
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
PrivateDevices=yes
ReadWritePaths=/var/lib/myapp
CapabilityBoundingSet=
SystemCallFilter=@system-service
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

## Procedimiento para crear e instalar un servicio

```bash
# 1. Crear el archivo de unidad:
sudo tee /etc/systemd/system/myapp.service << 'EOF'
[Unit]
Description=My Application
After=network.target

[Service]
Type=simple
User=myapp
ExecStart=/opt/myapp/bin/server
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

# 2. Recargar systemd para que lea la nueva unidad:
sudo systemctl daemon-reload

# 3. Verificar que systemd la reconoce:
systemctl list-unit-files | grep myapp

# 4. Verificar sintaxis (sin iniciar):
systemd-analyze verify /etc/systemd/system/myapp.service

# 5. Iniciar el servicio:
sudo systemctl start myapp.service

# 6. Verificar estado:
systemctl status myapp.service -l --no-pager

# 7. Ver logs si algo falla:
journalctl -u myapp.service --no-pager -n 50

# 8. Habilitar al boot (si funciona):
sudo systemctl enable myapp.service

# 9. Verificar enable:
systemctl is-enabled myapp.service
```

## Seguridad — Hardening

```ini
[Service]
# Restringir el sistema de archivos:
ProtectSystem=strict          # /usr, /boot, /etc read-only
ProtectHome=yes               # /home, /root, /run/user inaccesibles
PrivateTmp=yes                # /tmp aislado para este servicio
ReadWritePaths=/var/lib/myapp  # excepciones explícitas

# Restringir privilegios:
NoNewPrivileges=yes           # no puede ganar nuevos privilegios
CapabilityBoundingSet=        # sin capabilities extra (vacío = ninguna)

# Restringir red:
PrivateNetwork=yes            # sin acceso a la red (para tareas locales)
# O más granular:
RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX

# Restringir syscalls:
SystemCallFilter=@system-service  # solo syscalls típicas de servicios
SystemCallArchitectures=native    # solo la arquitectura actual

# Restringir dispositivos:
PrivateDevices=yes            # sin acceso a /dev (excepto /dev/null etc.)
```

```bash
# Ver el nivel de seguridad de un servicio:
systemd-analyze security myapp.service
# OVERALL EXPOSURE LEVEL: 4.2 MEDIUM
# Muestra cada directiva y su impacto
# Escala: 0.0 (más seguro) → 10.0 (menos seguro)
#   0.0-2.0  SAFE
#   2.0-4.0  OK
#   4.0-7.0  MEDIUM
#   7.0-10.0 UNSAFE
```

## systemctl edit — Overrides

```bash
# Crear drop-in (override parcial):
sudo systemctl edit myapp.service
# Abre editor, guarda en /etc/systemd/system/myapp.service.d/override.conf
# Ejecuta daemon-reload automáticamente

# Override completo (copia entera a /etc/):
sudo systemctl edit --full myapp.service
# Ventaja: control total
# Desventaja: pierde updates futuros del paquete

# Ver resultado combinado:
systemctl cat myapp.service
```

## Verificación y diagnóstico

```bash
# Verificar sintaxis (sin iniciar):
systemd-analyze verify /etc/systemd/system/myapp.service

# Ver tiempo de startup:
systemd-analyze critical-chain myapp.service

# Generar grafo de dependencias:
systemd-analyze dot | dot -Tsvg > deps.svg

# Si el servicio no arranca:
systemctl status myapp -l --no-pager        # estado y logs recientes
journalctl -u myapp --no-pager -n 100       # logs completos
systemctl show myapp --property=Result,ExecMainCode,ExecMainStatus
```

## Solución de problemas comunes

### Servicio arranca pero muere inmediatamente

```bash
# Si usa Type=simple, verificar que ExecStart NO termina
# (debe ser un proceso que corre indefinidamente)

# Si usa Type=notify, verificar que el proceso envía sd_notify(READY=1)

# Ver los logs:
journalctl -u myapp.service -f
```

### Error: "Unit not found"

```bash
# 1. Verificar que el archivo existe:
ls -la /etc/systemd/system/myapp.service

# 2. Recargar:
sudo systemctl daemon-reload

# 3. Verificar que se cargó:
systemctl list-unit-files | grep myapp
```

---

## Lab — Crear una unidad de servicio custom

### Objetivo

Dominar la creación de servicios systemd: los diferentes Type=
(simple, oneshot, forking, notify), comandos de ejecución con
prefijos especiales, configuración de usuario/entorno, directorios
gestionados, hardening de seguridad, y validación con
systemd-analyze verify/security.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Type y ExecStart

#### Paso 1.1: Type=simple (default)

```bash
docker compose exec debian-dev bash -c '
echo "=== Type=simple ==="
echo ""
cat > /tmp/lab-simple.service << '\''EOF'\''
[Unit]
Description=Simple Daemon Example

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
echo "--- Unit file ---"
cat -n /tmp/lab-simple.service
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-simple.service 2>&1 || true
echo ""
echo "Type=simple:"
echo "  El proceso de ExecStart ES el servicio"
echo "  systemctl start retorna inmediatamente"
echo "  systemd asume que está listo cuando ExecStart se ejecuta"
echo "  Es el Type por defecto (si no se especifica)"
'
```

#### Paso 1.2: Type=oneshot

```bash
docker compose exec debian-dev bash -c '
echo "=== Type=oneshot ==="
echo ""
cat > /tmp/lab-oneshot.service << '\''EOF'\''
[Unit]
Description=Database Cleanup (oneshot)
After=network.target

[Service]
Type=oneshot
ExecStart=/usr/bin/echo "Cleanup completed"
RemainAfterExit=yes
StandardOutput=journal

[Install]
WantedBy=multi-user.target
EOF
echo "--- Unit file ---"
cat -n /tmp/lab-oneshot.service
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-oneshot.service 2>&1 || true
echo ""
echo "Type=oneshot:"
echo "  systemd espera a que ExecStart termine"
echo "  RemainAfterExit=yes: queda active (exited) al terminar"
echo "  Útil para tareas de inicialización y scripts de setup"
echo "  Puede tener múltiples ExecStart= (se ejecutan en orden)"
'
```

#### Paso 1.3: Type=forking y Type=notify

```bash
docker compose exec debian-dev bash -c '
echo "=== Type=forking y Type=notify ==="
echo ""
echo "--- Type=forking (legacy) ---"
cat > /tmp/lab-forking.service << '\''EOF'\''
[Unit]
Description=Forking Daemon Example

[Service]
Type=forking
ExecStart=/opt/myapp/bin/server --daemon
PIDFile=/run/myapp.pid

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-forking.service
echo ""
echo "forking: el proceso hace fork() y el padre sale"
echo "  systemd espera a que el padre salga"
echo "  PIDFile= indica dónde encontrar el PID del hijo (el daemon)"
echo "  Es el modelo LEGACY — evitar si es posible"
echo ""
echo "--- Type=notify (moderno) ---"
cat > /tmp/lab-notify.service << '\''EOF'\''
[Unit]
Description=Notify Daemon Example

[Service]
Type=notify
ExecStart=/opt/myapp/bin/server
WatchdogSec=30

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-notify.service
echo ""
echo "notify: el proceso envía sd_notify(READY=1) cuando está listo"
echo "  systemctl start espera la notificación"
echo "  WatchdogSec= habilita health checks"
echo "  El servicio debe usar la API sd_notify (libsystemd)"
'
```

#### Paso 1.4: ExecStart y prefijos

```bash
docker compose exec debian-dev bash -c '
echo "=== ExecStart y prefijos ==="
echo ""
echo "ExecStart DEBE usar ruta absoluta (no busca en PATH)"
echo ""
echo "--- Comandos disponibles ---"
echo "ExecStartPre=   antes de arrancar (múltiples, en orden)"
echo "ExecStart=      comando principal"
echo "ExecStartPost=  después de arrancar"
echo "ExecReload=     al hacer reload (típicamente: kill -HUP \$MAINPID)"
echo "ExecStop=       al hacer stop (default: SIGTERM)"
echo "ExecStopPost=   después de detenerse"
echo ""
echo "--- Prefijos especiales ---"
echo "ExecStart=-/path   ignorar exit code (no marcar failed)"
echo "ExecStart=+/path   ejecutar con privilegios completos"
echo "ExecStart=!/path   sin ambient capabilities"
echo ""
echo "--- Ejemplo completo ---"
cat > /tmp/lab-full.service << '\''EOF'\''
[Unit]
Description=Full Example Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStartPre=/usr/bin/echo "Checking config..."
ExecStartPre=/usr/bin/test -f /etc/hostname
ExecStart=/usr/bin/sleep infinity
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/usr/bin/echo "Stopping..."

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-full.service
echo ""
systemd-analyze verify /tmp/lab-full.service 2>&1 || true
'
```

---

### Parte 2 — Entorno y directorios

#### Paso 2.1: User, Group y WorkingDirectory

```bash
docker compose exec debian-dev bash -c '
echo "=== User, Group, WorkingDirectory ==="
echo ""
cat > /tmp/lab-user.service << '\''EOF'\''
[Unit]
Description=Service with User/Group

[Service]
Type=simple
User=nobody
Group=nogroup
WorkingDirectory=/tmp
ExecStart=/usr/bin/sleep infinity
UMask=0077
StandardOutput=journal
SyslogIdentifier=lab-user

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-user.service
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-user.service 2>&1 || true
echo ""
echo "User=/Group= definen con qué permisos corre el proceso"
echo "WorkingDirectory= define el directorio de trabajo"
echo "UMask= define el umask (default: 0022)"
echo "SyslogIdentifier= nombre en los logs del journal"
'
```

#### Paso 2.2: Variables de entorno

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables de entorno ==="
echo ""
echo "--- Inline ---"
cat > /tmp/lab-env.service << '\''EOF'\''
[Unit]
Description=Service with Environment

[Service]
Type=simple
Environment=NODE_ENV=production
Environment="PORT=3000" "HOST=0.0.0.0"
ExecStart=/usr/bin/env
StandardOutput=journal

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-env.service
echo ""
echo "--- EnvironmentFile ---"
echo "Para variables sensibles, usar un archivo:"
cat > /tmp/lab-envfile << '\''EOF'\''
DATABASE_URL=postgresql://localhost/myapp
SECRET_KEY=abc123
LOG_LEVEL=info
# Líneas con # son comentarios
# SIN export, SIN comillas (a menos que el valor las contenga)
EOF
echo ""
echo "Contenido de /tmp/lab-envfile:"
cat /tmp/lab-envfile
echo ""
cat > /tmp/lab-envfile.service << '\''EOF'\''
[Unit]
Description=Service with EnvironmentFile

[Service]
Type=simple
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local
ExecStart=/usr/bin/env

[Install]
WantedBy=multi-user.target
EOF
echo "--- Unit file ---"
cat -n /tmp/lab-envfile.service
echo ""
echo "EnvironmentFile=-/path  (con -)  es opcional, no falla si no existe"
echo "EnvironmentFile=/path   (sin -)  falla si no existe"
rm /tmp/lab-envfile
'
```

#### Paso 2.3: Directorios gestionados

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorios gestionados ==="
echo ""
echo "systemd puede crear directorios automáticamente"
echo "con los permisos correctos:"
echo ""
echo "  StateDirectory=myapp         /var/lib/myapp"
echo "  CacheDirectory=myapp         /var/cache/myapp"
echo "  LogsDirectory=myapp          /var/log/myapp"
echo "  ConfigurationDirectory=myapp /etc/myapp (read-only)"
echo "  RuntimeDirectory=myapp       /run/myapp (se borra al parar)"
echo ""
cat > /tmp/lab-dirs.service << '\''EOF'\''
[Unit]
Description=Service with Managed Directories

[Service]
Type=simple
User=nobody
ExecStart=/usr/bin/sleep infinity
StateDirectory=lab-demo
RuntimeDirectory=lab-demo
CacheDirectory=lab-demo

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-dirs.service
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-dirs.service 2>&1 || true
echo ""
echo "Los directorios se crean con ownership del User= configurado"
echo "RuntimeDirectory se elimina automáticamente al parar el servicio"
'
```

---

### Parte 3 — Hardening y verificación

#### Paso 3.1: Directivas de seguridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Hardening de servicios ==="
echo ""
cat > /tmp/lab-hardened.service << '\''EOF'\''
[Unit]
Description=Hardened Service Example

[Service]
Type=simple
User=nobody
Group=nogroup
ExecStart=/usr/bin/sleep infinity

# Filesystem
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
ReadWritePaths=/var/lib/myapp

# Privilegios
NoNewPrivileges=yes
CapabilityBoundingSet=

# Red (descomentar si el servicio no necesita red)
# PrivateNetwork=yes
RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX

# Syscalls
SystemCallArchitectures=native

# Dispositivos
PrivateDevices=yes

[Install]
WantedBy=multi-user.target
EOF
echo "--- Unit file hardened ---"
cat -n /tmp/lab-hardened.service
echo ""
echo "--- Directivas clave ---"
echo "ProtectSystem=strict    /usr, /boot, /etc read-only"
echo "ProtectHome=yes         /home, /root inaccesibles"
echo "PrivateTmp=yes          /tmp aislado para este servicio"
echo "NoNewPrivileges=yes     no puede ganar nuevos privilegios"
echo "PrivateDevices=yes      sin acceso a /dev"
echo "CapabilityBoundingSet=  sin capabilities extra (vacío = ninguna)"
'
```

#### Paso 3.2: systemd-analyze security

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze security ==="
echo ""
echo "Evalúa el nivel de seguridad de un servicio"
echo "Muestra cada directiva y su impacto"
echo ""
echo "--- Servicio sin hardening ---"
systemd-analyze security /tmp/lab-full.service 2>&1 | tail -5 || \
    echo "(systemd-analyze security no disponible en este entorno)"
echo ""
echo "--- Servicio con hardening ---"
systemd-analyze security /tmp/lab-hardened.service 2>&1 | tail -5 || \
    echo "(systemd-analyze security no disponible en este entorno)"
echo ""
echo "La puntuación va de 0.0 (más seguro) a 10.0 (menos seguro)"
echo "OVERALL EXPOSURE LEVEL:"
echo "  0.0-2.0  SAFE"
echo "  2.0-4.0  OK"
echo "  4.0-7.0  MEDIUM"
echo "  7.0-10.0 UNSAFE"
'
```

#### Paso 3.3: Ejemplo completo — servicio de producción

```bash
docker compose exec debian-dev bash -c '
echo "=== Servicio de producción completo ==="
echo ""
cat > /tmp/lab-production.service << '\''EOF'\''
[Unit]
Description=Production API Server
Documentation=https://example.com/docs
After=network-online.target postgresql.service
Wants=network-online.target
Requires=postgresql.service
StartLimitIntervalSec=600
StartLimitBurst=5

[Service]
Type=notify
User=apiserver
Group=apiserver

ExecStartPre=/opt/api/bin/check-config /etc/api/config.yaml
ExecStart=/opt/api/bin/server
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure
RestartSec=10
WatchdogSec=30

TimeoutStartSec=60
TimeoutStopSec=30

StateDirectory=api
RuntimeDirectory=api
EnvironmentFile=/etc/api/env
EnvironmentFile=-/etc/api/env.local

LimitNOFILE=65535
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
NoNewPrivileges=yes
ReadWritePaths=/var/lib/api

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-production.service
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-production.service 2>&1 || true
echo ""
echo "Este unit file cubre:"
echo "  - Dependencias (After, Wants, Requires)"
echo "  - Pre-checks (ExecStartPre)"
echo "  - Restart con límites (on-failure, StartLimit)"
echo "  - Watchdog (WatchdogSec)"
echo "  - Timeouts (start y stop)"
echo "  - Directorios gestionados (State, Runtime)"
echo "  - Variables de entorno (EnvironmentFile)"
echo "  - Hardening (Protect*, NoNewPrivileges)"
'
```

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/lab-simple.service /tmp/lab-oneshot.service
rm -f /tmp/lab-forking.service /tmp/lab-notify.service
rm -f /tmp/lab-full.service /tmp/lab-user.service
rm -f /tmp/lab-env.service /tmp/lab-envfile.service
rm -f /tmp/lab-dirs.service /tmp/lab-hardened.service
rm -f /tmp/lab-production.service
echo "Archivos temporales eliminados"
'
```

### Conceptos reforzados

1. `Type=` define cómo systemd detecta que el servicio arrancó:
   `simple` (el proceso ES el servicio), `oneshot` (ejecuta y
   termina), `forking` (legacy, hace fork), `notify` (envía
   sd_notify).

2. `ExecStart=` requiere ruta absoluta. Los prefijos `-` (ignorar
   exit code), `+` (privilegios completos) modifican el
   comportamiento. `ExecStartPre=` puede ser múltiple.

3. `EnvironmentFile=` carga variables desde un archivo (formato
   `KEY=VALUE`, sin `export`). El prefijo `-` lo hace opcional.

4. Los directorios gestionados (`StateDirectory=`, `RuntimeDirectory=`)
   se crean automáticamente con ownership correcto.
   `RuntimeDirectory=` se elimina al parar el servicio.

5. Las directivas de hardening (`ProtectSystem=strict`,
   `ProtectHome=yes`, `PrivateTmp=yes`, `NoNewPrivileges=yes`)
   restringen el acceso del servicio al filesystem y privilegios.

6. `systemd-analyze verify` valida la sintaxis y `systemd-analyze
   security` evalúa el nivel de seguridad (0.0=seguro, 10.0=inseguro).

---

## Ejercicios

### Ejercicio 1 — Crear un servicio Type=simple

Crea un servicio que ejecute un loop infinito escribiendo la fecha al journal cada 10 segundos. Incluye [Unit] con Description y After=network.target, [Service] con Type=simple y Restart=on-failure, y [Install] con WantedBy=multi-user.target.

```bash
# 1. Crear el script:
sudo tee /usr/local/bin/hello-service.sh << 'EOF'
#!/bin/bash
while true; do
    echo "$(date): Hello from systemd service"
    sleep 10
done
EOF
sudo chmod +x /usr/local/bin/hello-service.sh

# 2. Crear la unidad:
sudo tee /etc/systemd/system/hello.service << 'EOF'
[Unit]
Description=Hello World Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/hello-service.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# 3. Probar:
sudo systemctl daemon-reload
sudo systemctl start hello
systemctl status hello -l --no-pager
journalctl -u hello --no-pager -n 5
```

<details><summary>Predicción</summary>

- `daemon-reload` hace que systemd lea el nuevo archivo `.service`
- `start hello` ejecuta `/usr/local/bin/hello-service.sh` como proceso principal
- `status` muestra: `● hello.service - Hello World Service`, `Active: active (running)`, y en CGroup el proceso del script con su PID
- `journalctl` muestra las líneas de "Hello from systemd service" con timestamps
- Como es `Type=simple`, systemd considera el servicio activo inmediatamente al ejecutar ExecStart
- Si matas el proceso con `kill`, `Restart=on-failure` lo reinicia automáticamente después de `RestartSec=5`

Limpiar con:
```bash
sudo systemctl stop hello
sudo systemctl disable hello
sudo rm /etc/systemd/system/hello.service /usr/local/bin/hello-service.sh
sudo systemctl daemon-reload
```

</details>

### Ejercicio 2 — Crear un servicio Type=oneshot

Crea un servicio oneshot que escriba "Initialization completed at DATE" en `/var/log/myapp-init.log`. Usa `RemainAfterExit=yes`. ¿Qué estado muestra `systemctl status` después de que termina?

```bash
sudo tee /usr/local/bin/init-log.sh << 'EOF'
#!/bin/bash
echo "Initialization completed at $(date)" >> /var/log/myapp-init.log
EOF
sudo chmod +x /usr/local/bin/init-log.sh

sudo tee /etc/systemd/system/init-log.service << 'EOF'
[Unit]
Description=Initialize Application Log

[Service]
Type=oneshot
ExecStart=/usr/local/bin/init-log.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl start init-log
systemctl status init-log --no-pager
```

<details><summary>Predicción</summary>

- El script se ejecuta una vez y termina
- Gracias a `RemainAfterExit=yes`, el estado queda `active (exited)` — no `inactive`
- Sin `RemainAfterExit=yes`, quedaría `inactive (dead)` inmediatamente
- Esto es útil para tareas de inicialización: otros servicios pueden depender de esta unidad (con `After=init-log.service`) y verla como "active"
- `systemctl start init-log` espera a que ExecStart termine (a diferencia de simple, que retorna inmediatamente)

Limpiar con:
```bash
sudo systemctl stop init-log
sudo rm /etc/systemd/system/init-log.service /usr/local/bin/init-log.sh
sudo rm -f /var/log/myapp-init.log
sudo systemctl daemon-reload
```

</details>

### Ejercicio 3 — EnvironmentFile

Crea un servicio que lea variables desde un `EnvironmentFile`. El script debe imprimir las variables al journal. Incluye un segundo `EnvironmentFile` con prefijo `-` (opcional). ¿Qué pasa si el archivo opcional no existe?

```bash
# Crear directorio y archivo de entorno:
sudo mkdir -p /etc/myapp
sudo tee /etc/myapp/env << 'EOF'
ENVIRONMENT=production
LOG_LEVEL=info
PORT=3000
EOF

# Crear script:
sudo tee /usr/local/bin/env-demo.sh << 'EOF'
#!/bin/bash
echo "ENVIRONMENT=$ENVIRONMENT"
echo "LOG_LEVEL=$LOG_LEVEL"
echo "PORT=$PORT"
echo "EXTRA=$EXTRA"
while true; do sleep 60; done
EOF
sudo chmod +x /usr/local/bin/env-demo.sh

# Crear unidad:
sudo tee /etc/systemd/system/env-demo.service << 'EOF'
[Unit]
Description=Environment Demo Service

[Service]
Type=simple
ExecStart=/usr/local/bin/env-demo.sh
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl start env-demo
journalctl -u env-demo --no-pager -n 10
```

<details><summary>Predicción</summary>

- El servicio arranca correctamente aunque `/etc/myapp/env.local` no existe, gracias al prefijo `-`
- Sin el `-`, obtendríamos error: el servicio no arrancaría
- Los logs muestran las variables leídas: `ENVIRONMENT=production`, `LOG_LEVEL=info`, `PORT=3000`, `EXTRA=` (vacío, porque no está definida)
- El formato del EnvironmentFile es `KEY=VALUE` sin `export`, sin comillas, líneas con `#` son comentarios
- Si creamos `/etc/myapp/env.local` con `EXTRA=debug` y reiniciamos, aparecería

Limpiar con:
```bash
sudo systemctl stop env-demo
sudo rm /etc/systemd/system/env-demo.service /usr/local/bin/env-demo.sh
sudo rm -rf /etc/myapp
sudo systemctl daemon-reload
```

</details>

### Ejercicio 4 — Directorios gestionados

Crea un servicio que use `StateDirectory=`, `RuntimeDirectory=`, y `CacheDirectory=`. Después de iniciar el servicio, verifica que los directorios existen con los permisos correctos. ¿Qué pasa con `RuntimeDirectory` al parar el servicio?

```bash
sudo useradd -r -s /sbin/nologin demoapp 2>/dev/null || true

sudo tee /etc/systemd/system/dirs-demo.service << 'EOF'
[Unit]
Description=Managed Directories Demo

[Service]
Type=simple
User=demoapp
ExecStart=/usr/bin/sleep infinity
StateDirectory=dirs-demo
RuntimeDirectory=dirs-demo
CacheDirectory=dirs-demo

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl start dirs-demo

# Verificar directorios:
ls -ld /var/lib/dirs-demo /run/dirs-demo /var/cache/dirs-demo
```

<details><summary>Predicción</summary>

Los tres directorios se crean automáticamente:
- `/var/lib/dirs-demo` — owner `demoapp:demoapp`, modo 0755 (persiste al parar)
- `/run/dirs-demo` — owner `demoapp:demoapp`, modo 0755 (**se elimina al parar**)
- `/var/cache/dirs-demo` — owner `demoapp:demoapp`, modo 0755 (persiste al parar)

Al ejecutar `sudo systemctl stop dirs-demo`:
- `/run/dirs-demo` desaparece (RuntimeDirectory se limpia automáticamente)
- `/var/lib/dirs-demo` y `/var/cache/dirs-demo` permanecen

Esto elimina la necesidad de `ExecStartPre=mkdir` y `ExecStopPost=rm`.

Limpiar con:
```bash
sudo systemctl stop dirs-demo
sudo rm /etc/systemd/system/dirs-demo.service
sudo rm -rf /var/lib/dirs-demo /var/cache/dirs-demo
sudo userdel demoapp 2>/dev/null || true
sudo systemctl daemon-reload
```

</details>

### Ejercicio 5 — ExecStartPre con prefijo -

Crea un servicio con dos `ExecStartPre`. El primero ejecuta un comando que falla (ej: `test -f /tmp/no-existe`), el segundo ejecuta uno que tiene éxito. ¿Arranca el servicio? ¿Qué cambia si agregas el prefijo `-` al primero?

```bash
# Sin prefijo - (falla):
sudo tee /etc/systemd/system/precheck-demo.service << 'EOF'
[Unit]
Description=Pre-check Demo

[Service]
Type=simple
ExecStartPre=/usr/bin/test -f /tmp/no-existe
ExecStartPre=/usr/bin/echo "Second pre-check passed"
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl start precheck-demo
systemctl status precheck-demo --no-pager
```

<details><summary>Predicción</summary>

Sin `-`:
- El primer `ExecStartPre` falla (`/tmp/no-existe` no existe → exit code 1)
- El segundo `ExecStartPre` no se ejecuta
- `ExecStart` no se ejecuta
- El servicio queda en estado `failed`
- `status` muestra `code=exited, status=1/FAILURE`

Si cambiamos a `ExecStartPre=-/usr/bin/test -f /tmp/no-existe`:
- El prefijo `-` hace que el exit code se ignore
- El primer pre-check "falla" pero se ignora
- El segundo pre-check se ejecuta
- `ExecStart` se ejecuta normalmente
- El servicio queda `active (running)`

Esto es útil para checks opcionales (ej: limpiar un archivo temporal que puede o no existir).

</details>

### Ejercicio 6 — Identificar Type de servicios del sistema

Para cada servicio activo del sistema, identifica qué `Type=` usa. ¿Cuáles usan `notify`? ¿Cuáles usan `forking`? ¿Por qué un servicio usaría `forking` en vez de `simple`?

```bash
for svc in sshd cron systemd-journald systemd-logind dbus; do
    TYPE=$(systemctl show "$svc" --property=Type --value 2>/dev/null)
    PIDFILE=$(systemctl show "$svc" --property=PIDFile --value 2>/dev/null)
    if [[ -n "$TYPE" ]]; then
        printf "%-25s Type=%-10s PIDFile=%s\n" "$svc" "$TYPE" "$PIDFILE"
    fi
done
```

<details><summary>Predicción</summary>

Resultados típicos:
- `sshd` → `Type=notify` (sshd moderno usa sd_notify para indicar readiness)
- `cron` → `Type=simple` o `Type=forking` (depende de la distribución)
- `systemd-journald` → `Type=notify` (componente interno de systemd con soporte sd_notify)
- `systemd-logind` → `Type=notify` (igual, componente de systemd)
- `dbus` → `Type=dbus` o `Type=notify` (depende de la versión)

Un servicio usa `forking` cuando el binario sigue el patrón de daemon clásico de SysV: el proceso padre hace fork(), el hijo continúa como daemon, y el padre sale. Esto indica a systemd que el arranque está completo. Es el modelo legacy — los servicios modernos prefieren `notify` o `simple`.

</details>

### Ejercicio 7 — Hardening básico

Toma un servicio sin hardening y agrégale directivas de seguridad. Usa `systemd-analyze security` antes y después para ver el cambio en la puntuación.

```bash
# Servicio base (sin hardening):
sudo tee /etc/systemd/system/secure-test.service << 'EOF'
[Unit]
Description=Security Test Service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=multi-user.target
EOF

echo "=== Sin hardening ==="
systemd-analyze security secure-test.service 2>&1 | tail -3

# Agregar hardening:
sudo tee /etc/systemd/system/secure-test.service << 'EOF'
[Unit]
Description=Security Test Service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
User=nobody
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
PrivateDevices=yes
CapabilityBoundingSet=
SystemCallArchitectures=native

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
echo "=== Con hardening ==="
systemd-analyze security secure-test.service 2>&1 | tail -3
```

<details><summary>Predicción</summary>

Sin hardening (User=root, sin restricciones):
- Puntuación alta: ~9.6 UNSAFE
- Casi todas las directivas aparecen como "EXPOSED"

Con hardening:
- Puntuación baja: ~3.0-4.0 OK/MEDIUM
- Las directivas aplicadas aparecen como "OK"
- Quedan algunas en "EXPOSED" (como network access, si no se usa PrivateNetwork)

El impacto más grande lo dan:
- `User=nobody` (no correr como root)
- `NoNewPrivileges=yes` (previene escalación)
- `ProtectSystem=strict` (filesystem read-only)
- `CapabilityBoundingSet=` (sin capabilities)

Limpiar:
```bash
sudo rm /etc/systemd/system/secure-test.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 8 — Watchdog

¿Qué hace `WatchdogSec=` en un servicio? ¿Qué pasa si un servicio con `WatchdogSec=30` deja de responder? ¿Con qué Type= tiene sentido usar WatchdogSec?

```bash
# Examinar un servicio que usa watchdog:
systemctl show systemd-journald --property=WatchdogUSec,Type --value 2>/dev/null
```

<details><summary>Predicción</summary>

`WatchdogSec=30` significa:
- El servicio debe llamar `sd_notify("WATCHDOG=1")` cada 30 segundos
- Si no lo hace, systemd considera que el servicio está colgado
- systemd lo mata y lo reinicia (si Restart= lo permite)

Solo tiene sentido con `Type=notify` porque:
- El servicio debe usar la API `sd_notify()` para enviar los pings de watchdog
- `Type=simple` no soporta sd_notify (el servicio no sabe comunicarse con systemd)
- `Type=forking` tampoco (el padre ya salió)

`systemd-journald` típicamente muestra `WatchdogUSec` con un valor (en microsegundos) y `Type=notify`, confirmando que usa el watchdog.

Si el servicio se cuelga:
1. El timeout de watchdog expira
2. systemd registra `Result=watchdog`
3. Si `Restart=on-failure` o `Restart=always`, se reinicia automáticamente

</details>

### Ejercicio 9 — systemd-analyze verify

Crea un unit file con errores deliberados y usa `systemd-analyze verify` para identificarlos. Prueba con: ruta relativa en ExecStart, referencia a un target inexistente, y Type inválido.

```bash
# Unit file con errores:
sudo tee /etc/systemd/system/broken.service << 'EOF'
[Unit]
Description=Broken Service
After=nonexistent.target

[Service]
Type=invalid
ExecStart=relative/path/server
User=nonexistent_user_12345

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
systemd-analyze verify /etc/systemd/system/broken.service 2>&1
```

<details><summary>Predicción</summary>

`systemd-analyze verify` reportará múltiples errores:

1. `After=nonexistent.target` — advertencia de que la unidad referenciada no existe (warning, no error fatal)
2. `Type=invalid` — error: tipo no reconocido
3. `ExecStart=relative/path/server` — error: ExecStart debe ser una ruta absoluta
4. `User=nonexistent_user_12345` — puede generar advertencia sobre usuario inexistente

El exit code será no-cero indicando problemas. `verify` es útil para validar la sintaxis **antes** de intentar iniciar el servicio, evitando descubrir errores en producción.

Limpiar:
```bash
sudo rm /etc/systemd/system/broken.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 10 — Servicio completo de producción

Diseña un unit file para un servidor API hipotético que cumpla estos requisitos:
- Arranca después de red y PostgreSQL
- Usa Type=notify con WatchdogSec
- Ejecuta un pre-check de configuración
- Tiene EnvironmentFile (obligatorio) y uno opcional
- Usa directorios gestionados (State y Runtime)
- Incluye hardening básico
- Se reinicia en caso de fallo con límite de 5 intentos en 10 minutos

```bash
sudo tee /etc/systemd/system/api-server.service << 'EOF'
[Unit]
Description=Production API Server
Documentation=https://docs.example.com/api
After=network-online.target postgresql.service
Wants=network-online.target
Requires=postgresql.service
StartLimitIntervalSec=600
StartLimitBurst=5

[Service]
Type=notify
User=apiserver
Group=apiserver

ExecStartPre=/opt/api/bin/check-config /etc/api/config.yaml
ExecStart=/opt/api/bin/server
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure
RestartSec=10
WatchdogSec=30
TimeoutStartSec=60
TimeoutStopSec=30

EnvironmentFile=/etc/api/env
EnvironmentFile=-/etc/api/env.local
StateDirectory=api
RuntimeDirectory=api
LimitNOFILE=65535

NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=yes
PrivateDevices=yes
ReadWritePaths=/var/lib/api

[Install]
WantedBy=multi-user.target
EOF

systemd-analyze verify /etc/systemd/system/api-server.service 2>&1
```

<details><summary>Predicción</summary>

Este unit file cubre todos los aspectos de un servicio de producción:

- **Dependencias**: `After` + `Requires=postgresql.service` → no arranca sin PostgreSQL. `Wants=network-online.target` → espera red disponible pero no falla si tarda.
- **StartLimit**: máximo 5 intentos en 600s (10 min). Tras 5 fallos → estado `start-limit-hit` y deja de reiniciar.
- **Type=notify**: `systemctl start` espera hasta que el servidor envíe `sd_notify(READY=1)`. `WatchdogSec=30` requiere ping cada 30s.
- **ExecStartPre**: valida la configuración antes de arrancar. Si falla → el servicio no inicia.
- **Restart=on-failure** + **RestartSec=10**: se reinicia tras fallo, esperando 10s entre intentos.
- **Timeouts**: 60s para arrancar, 30s para parar. Si excede → SIGKILL.
- **EnvironmentFile**: uno obligatorio, uno opcional con `-`.
- **Directorios**: `StateDirectory` crea `/var/lib/api` persistente. `RuntimeDirectory` crea `/run/api` efímero.
- **Hardening**: filesystem read-only excepto `/var/lib/api`, sin escalación de privilegios, tmp aislado, sin dispositivos.

`verify` puede mostrar warnings sobre unidades/binarios que no existen en el sistema actual, lo cual es esperado para un servicio hipotético.

</details>
