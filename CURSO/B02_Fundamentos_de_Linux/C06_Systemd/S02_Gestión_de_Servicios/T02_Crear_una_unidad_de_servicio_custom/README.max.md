# T02 — Crear una unidad de servicio custom

## Anatomía completa de un .service

```
┌─────────────────────────────────────────────────────────────┐
│               /etc/systemd/system/myapp.service               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  [Unit]                                                      │
│      Description=My Application Server                       │
│      Documentation=https://example.com/docs                  │
│      After=network.target postgresql.service                  │
│      Wants=postgresql.service                                │
│      ConditionPathExists=/etc/myapp/config.yaml             │
│                                                              │
│  [Service]                                                   │
│      Type=notify                                             │
│      User=myapp                                             │
│      Group=myapp                                            │
│      WorkingDirectory=/opt/myapp                            │
│      ExecStart=/opt/myapp/bin/server --config /etc/myapp/config.yaml │
│      ExecReload=/bin/kill -HUP $MAINPID                     │
│      Restart=on-failure                                      │
│      RestartSec=10                                           │
│      WatchdogSec=30                                          │
│      Environment="PORT=3000"                                  │
│      EnvironmentFile=/etc/myapp/env                          │
│      StateDirectory=myapp                                   │
│      RuntimeDirectory=myapp                                  │
│      LogsDirectory=myapp                                    │
│                                                              │
│  [Install]                                                   │
│      WantedBy=multi-user.target                              │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

Las tres secciones:
- **[Unit]** → Metadata y relaciones (común a todos los tipos)
- **[Service]** → Configuración específica del servicio
- **[Install]** → Qué hacer al enable/disable

## Sección [Unit] — Detalle completo

### Description y Documentation

```ini
[Unit]
Description=My Application Server
# Visible en: systemctl status, journalctl -u, logs

Documentation=https://example.com/docs
Documentation=man:myapp(8)
Documentation=file:///usr/share/doc/myapp/README.md
# Múltiples valores separados por espacios o nuevas líneas
```

### Dependencias de orden (After/Before)

```ini
[Unit]
# After: esta unidad ARRANCAn después de las listadas
After=network.target postgresql.service remote-fs.target
# → postgresql se arranca primero (si está enabled)
# → Esta unidad espera a que postgresql esté active

# Before: esta unidad ARRANCAn antes de las listadas
Before=nginx.service
# → Esta unidad arranca ANTES de nginx

# After NO es una dependencia de activación — solo ordena
# Si postgresql no está enabled, esta unidad arranca igual
# Para crear una DEPENDENCIA real, usar Wants/Requires
```

### Dependencias de activación (Wants/Requires)

```ini
[Unit]
# Wants: desea activar las unidades listadas
# SI fallan, esta unidad ARRANCA IGUAL
Wants=postgresql.service redis.service
# → Intenta arrancar postgresql y redis
# → Pero si uno falla, myapp arranca igual

# Requires: requiere las unidades listadas
# SI alguna falla, esta unidad FALLA
Requires=postgresql.service
# → postgresql debe arrancar OK
# → Si postgresql falla, myapp también falla

# Combinar:
Wants=postgresql.service
After=postgresql.service
# → Intenta arrancar postgresql
# → Espera a que esté activo antes de arrancar myapp
# → myapp arranca aunque postgresql falle (Wants, no Requires)
```

### Conflicts — Mutuo exclusion

```ini
[Unit]
# Conflict: no puede coexistir con estas unidades
Conflicts=apache2.service
# → Si apache2 está corriendo, myapp NO puede arrancar
# → Al arrancar myapp, apache2 se detiene automáticamente
```

### Condiciones (Condition*)

Verifican algo antes de activar. Si fallan, la unidad se marca "skipped" silenciosamente:

```ini
[Unit]
# El archivo existe
ConditionPathExists=/etc/myapp/config.yaml

# El archivo tiene contenido
ConditionFileNotEmpty=/etc/myapp/config.yaml

# El directorio existe
ConditionDirectoryNotEmpty=/var/log/myapp

# El usuario existe
ConditionUser=myappuser

# Solo en cierta arquitectura
ConditionArchitecture=x86_64

# NO en contenedor
ConditionVirtualization=!container

# Solo en cierto host
ConditionHost=server1.example.com

# Si el kernel tiene cierto parameter de boot
ConditionKernelCommandLine=nomodeset
```

### Assertions — Como Condition pero falla ruidosamente

```ini
[Unit]
# Si falla, marca la unidad como "failed" con mensaje claro
AssertPathExists=/opt/myapp/bin/server
AssertUser=root
```

## Sección [Service] — Detalle completo

### Type — Cómo systemd detecta que el servicio arrancó

```
┌──────────────────────────────────────────────────────────────┐
│  Type=simple                                                 │
│  → ExecStart es el proceso principal                        │
│  → systemctl start retorna cuando ExecStart inicia         │
│  → Si ExecStart termina → servicio dead                    │
│  → USAR para servicios que corren continuamente            │
├──────────────────────────────────────────────────────────────┤
│  Type=exec                                                   │
│  → Como simple pero espera a que exec() tenga éxito       │
│  → Detecta binario no ejecutable, permisos, etc.           │
│  → Disponible en systemd 240+                             │
├──────────────────────────────────────────────────────────────┤
│  Type=forking                                                │
│  → ExecStart hace fork() y el padre termina                │
│  → systemd espera a que el padre termine                   │
│  → Necesita PIDFile para saber cuál es el hijo            │
│  → EVITAR — es el modelo legacy SysV                     │
├──────────────────────────────────────────────────────────────┤
│  Type=notify                                                 │
│  → El servicio envía sd_notify(READY=1) cuando está listo │
│  → systemctl start espera esa notificación                │
│  → El servicio debe usar libsystemd-dev o sd_notify()     │
├──────────────────────────────────────────────────────────────┤
│  Type=oneshot                                                │
│  → ExecStart CORRE Y TERMINA                              │
│  → RemainAfterExit=yes → queda "active" después           │
│  → Útil para scripts de setup/initialization              │
├──────────────────────────────────────────────────────────────┤
│  Type=dbus                                                   │
│  → El servicio toma un BusName en D-Bus                  │
│  → systemctl start espera hasta que el BusName exista     │
│  → Útil para servicios que ofrecen D-Bus APIs             │
└──────────────────────────────────────────────────────────────┘
```

```ini
# Ejemplo: simple (para nginx, apache, aplicaciones Node)
[Service]
Type=simple
ExecStart=/opt/myapp/bin/server
# systemd considera "active" cuando ExecStart se ejecuta

# Ejemplo: notify (para nginx con master/worker)
[Service]
Type=notify
ExecStart=/usr/sbin/nginx -g "daemon on;"
# systemd espera sd_notify(READY=1) desde el proceso

# Ejemplo: oneshot (script de inicialización)
[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/setup-database
RemainAfterExit=yes
# systemd marca "active (exited)" cuando ExecStart termina
# WantedBy=multi-user.target tiene sentido con RemainAfterExit=yes
```

### Comandos de ejecución (Exec*)

```ini
[Service]
# DEBE ser una ruta ABSOLUTA al ejecutable
ExecStart=/opt/myapp/bin/server --config /etc/myapp/config.yaml

# Ejecutados ANTES del principal (en orden):
ExecStartPre=/opt/myapp/bin/check-config
ExecStartPre=/usr/bin/mkdir -p /run/myapp
# Si cualquiera falla → el servicio no arranca

# Ejecutado DESPUÉS de ExecStart exitoso:
ExecStartPost=/opt/myapp/bin/notify-ready

# Para recargar (systemctl reload):
ExecReload=/bin/kill -HUP $MAINPID
# O: ExecReload=/opt/myapp/bin/server --reload

# Al detener (si no está definido: SIGTERM + SIGKILL después del timeout):
ExecStop=/opt/myapp/bin/server --graceful-shutdown
ExecStopPost=/bin/rm -f /run/myapp/socket

# El guión (-) antes del comando significa "ignorar exit code":
ExecStartPre=-/opt/myapp/bin/check-optional-dependency
# Si falla, el servicio NO se detiene (por el -)
```

### Variables disponibles en Exec*

```
$MAINPID         → PID del proceso principal (el de ExecStart)
$CONTEXT_NAME   → Nombre del contexto
$SERVICE_RESULT  → Result: success, failure, etc.
$TMPDIR         → Directorio temporal
$LOGO           → Logo del sistema
```

### Usuario, grupo y directorios gestionados

```ini
[Service]
User=myapp
Group=myapp

# Crear usuario dinámico (systemd 235+):
DynamicUser=yes
# → Crea usuario/grupo efímero para el servicio
# → Se elimina cuando el servicio para
# → NO es lo mismo que User=myapp

# Directorios gestionados automáticamente:
StateDirectory=myapp        # /var/lib/myapp (creado con permisos)
CacheDirectory=myapp        # /var/cache/myapp
LogsDirectory=myapp         # /var/log/myapp
ConfigurationDirectory=myapp # /etc/myapp (modo 0755)
RuntimeDirectory=myapp       # /run/myapp (limpio al parar)
```

```bash
# Beneficio de StateDirectory=:
# 1. systemd crea el directorio con el usuario correcto
# 2. Se elimina cuando el servicio para (RuntimeDirectory)
# 3. No necesitas ExecStartPre=mkdir
# 4. Funciona con DynamicUser=yes
```

### Variables de entorno

```ini
[Service]
# Directas:
Environment=NODE_ENV=production
Environment="PORT=3000"
Environment="SECRET=hello world"    # espacios requieren comillas

# Desde archivo (formato: KEY=VALUE por línea):
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local   # - = opcional si no existe

# Pasar variables del sistema:
PassEnvironment=LANG TZ
PassEnvironment=HOME
```

```bash
# /etc/myapp/env:
DATABASE_URL=postgresql://localhost/myapp
SECRET_KEY=abc123
LOG_LEVEL=info

# Formato:
# KEY=VALUE
# Sin export
# Sin comillas (a menos que sean parte del valor)
# Líneas con # son comentarios
```

### Restart y Watchdog

```ini
[Service]
# Restart: cuándo reiniciar automáticamente
Restart=no                     # nunca
Restart=on-success           # si exited con código 0
Restart=on-failure           # si exited con código != 0 o por señal
Restart=on-abnormal         # por señal (SIGTERM, etc.)
Restart=on-abort            # si el proceso muere sin clean exit
Restart=on-watchdog         # si watchdog no recibe ping
Restart=on-start-limit-hit  # si se superan los StartLimit

RestartSec=5                # segundos entre restart

# Watchdog: si el servicio no reporta vida, se reinicia
WatchdogSec=30              # segundos entre cada ping
# El servicio debe llamar sd_notify(READY=1 + WATCHDOG=1)
```

### Recursos y límites

```ini
[Service]
# Memoria máxima (bytes):
MemoryMax=512M
MemoryHigh=384M             # warning threshold

# Límite de CPUs (porcentaje, 100% = 1 core):
CPUQuota=50%               # medio core

# Límite de archivos abiertos:
LimitNOFILE=65535

# Otros límites:
LimitNPROC=1024             # procesos
LimitFSIZE=unlimited        # tamaño de archivo

# Límites "soft" (pueden aumentarse):
LimitCORE=infinity
```

## Sección [Install] — Detalle completo

```ini
[Install]
# WantedBy: el target que "quiere" esta unidad
WantedBy=multi-user.target
# Al enable: crea symlink en:
# /etc/systemd/system/multi-user.target.wants/myapp.service
# → /etc/systemd/system/myapp.service

# RequiredBy: dependencia fuerte (si falla, el target falla)
RequiredBy=graphical.target

# Also: unidades que se habilitan/deshabilitan junto con esta
Also=myapp.timer myapp-healthcheck.service
Also=myapp.socket

# Alias: nombres alternativos
Alias=webapp.service
# systemctl start webapp → myapp
```

## Ejemplos completos de servicios

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

# Recursos
MemoryMax=1G
CPUQuota=200%

# Directorios
RuntimeDirectory=nodeapp
LogsDirectory=nodeapp
StateDirectory=nodeapp

[Install]
WantedBy=multi-user.target
Also=nodeapp-healthcheck.service
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
After=network-online.target
Wants=network-online.target
After=postgresql.service

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

# Environment
Environment=NODE_ENV=production
EnvironmentFile=/etc/myapp/env

# Security hardening
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=true
PrivateDevices=true
ReadWritePaths=/opt/myapp/data /var/log/myapp
CapabilityBoundingSet=
SystemCallFilter=@system-service

# Resources
MemoryMax=512M
CPUQuota=50%
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

## Procedimiento para crear e instalar un servicio

```bash
# 1. Crear el archivo de unidad
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

# 2. Recargar systemd para que lea la nueva unidad
sudo systemctl daemon-reload

# 3. Verificar que systemd reconoce la unidad
systemctl list-unit-files | grep myapp

# 4. Verificar sintaxis (sin iniciar):
sudo systemd-analyze verify /etc/systemd/system/myapp.service

# 5. Iniciar el servicio
sudo systemctl start myapp.service

# 6. Verificar estado
systemctl status myapp.service -l --no-pager

# 7. Ver logs
journalctl -u myapp.service --no-pager -n 50

# 8. Si funciona, habilitar al boot
sudo systemctl enable myapp.service

# 9. Verificar enable
systemctl is-enabled myapp.service
```

## systemctl edit — Override correcto

```bash
# NO editar el archivo original del paquete directamente
# Usar drop-in overrides

# Crear override
sudo systemctl edit myapp.service
# Se guarda en: /etc/systemd/system/myapp.service.d/override.conf

# Agregar cambios:
# [Service]
# MemoryMax=1G
# Environment=DEBUG=1

# systemd hace daemon-reload automáticamente al guardar

# Reiniciar para que tome los cambios
sudo systemctl restart myapp.service

# Ver el resultado combinado
systemctl cat myapp.service
```

## systemd-analyze — Verificación y diagnóstico

```bash
# Verificar sintaxis de un archivo de unidad (sin aplicarla):
sudo systemd-analyze verify /etc/systemd/system/myapp.service
# Reporta: errores de sintaxis, referencias a units inexistentes, etc.
# Exit code = 0 si todo OK

# Ver tiempo de startup de una unidad:
systemd-analyze critical-chain myapp.service

# Ver Security exposure level:
systemd-analyze security myapp.service

# Dot output (para graficar dependencias):
systemctl dot | dot -Tsvg > deps.svg
```

## systemctl edit --full vs drop-in

```
Override completo (--full):
  Copia TODO el archivo a /etc/systemd/system/
  Ventaja: control total
  Desventaja: pierdes updates del paquete

Drop-in (edit sin --full):
  Solo sobrescribe directivas específicas
  El archivo original del paquete permanece intacto
  Ventaja: recibe updates del paquete
  Desventaja: solo puedes cambiar directivas individuales
```

## Solución de problemas

### Error: "Failed to start"

```bash
# 1. Ver el estado completo
systemctl status myapp.service -l --no-pager

# 2. Ver logs
journalctl -u myapp.service --no-pager -n 100
journalctl -u myapp.service -b  # logs del boot

# 3. Ver propiedades
systemctl show myapp.service --property=Result,ExecMainExitCode

# 4. Verificar el ejecutable existe
systemctl show myapp.service --property=ExecMainStartTimestamp
```

### Error: "Unit not found"

```bash
# 1. Verificar que el archivo existe
ls -la /etc/systemd/system/myapp.service

# 2. Recargar
sudo systemctl daemon-reload

# 3. Ver si se cargo
systemctl list-units | grep myapp
```

### Servicio arranca pero muere inmediatamente

```bash
# 1. Ver el estado
systemctl status myapp.service

# 2. Ver logs (puede estar truncado)
journalctl -u myapp.service -f

# 3. Si usa Type=simple, el problema es que ExecStart termina
# Solución: verificar que el proceso es un servidor que NO termina

# 4. Si usa Type=notify, verificar que envía sd_notify()
```

---

## Ejercicios

### Ejercicio 1 — Crear un servicio simple

```bash
# 1. Crear el script
sudo tee /usr/local/bin/hello-world.sh << 'EOF'
#!/bin/bash
while true; do
    echo "$(date): Hello from systemd service"
    sleep 5
done
EOF
sudo chmod +x /usr/local/bin/hello-world.sh

# 2. Crear la unidad
sudo tee /etc/systemd/system/hello-world.service << 'EOF'
[Unit]
Description=Hello World Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/hello-world.sh
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# 3. Recargar y probar
sudo systemctl daemon-reload
sudo systemctl start hello-world.service
systemctl status hello-world.service -l --no-pager

# 4. Ver logs
journalctl -u hello-world.service -f

# 5. Habilitar
sudo systemctl enable hello-world.service
systemctl is-enabled hello-world.service

# 6. Limpiar
sudo systemctl stop hello-world.service
sudo systemctl disable hello-world.service
sudo rm /etc/systemd/system/hello-world.service
sudo rm /usr/local/bin/hello-world.sh
sudo systemctl daemon-reload
```

### Ejercicio 2 — Servicio con EnvironmentFile

```bash
# 1. Crear directorio
sudo mkdir -p /opt/myapp
sudo useradd -r -s /sbin/nologin myapp 2>/dev/null || true

# 2. Crear script
sudo tee /opt/myapp/server.sh << 'EOF'
#!/bin/bash
source /etc/myapp/env
echo "Server started with ENV=${ENVIRONMENT:-default}"
while true; do
    echo "Running in ${ENVIRONMENT:-default}" 
    sleep 10
done
EOF
sudo chmod +x /opt/myapp/server.sh

# 3. Crear env file
echo "ENVIRONMENT=production" | sudo tee /etc/myapp/env

# 4. Crear unidad
sudo tee /etc/systemd/system/myapp.service << 'EOF'
[Unit]
Description=My Application with Environment
After=network.target

[Service]
Type=simple
User=myapp
Group=myapp
WorkingDirectory=/opt/myapp
ExecStart=/opt/myapp/server.sh
Restart=on-failure
EnvironmentFile=/etc/myapp/env

[Install]
WantedBy=multi-user.target
EOF

# 5. Probar
sudo systemctl daemon-reload
sudo systemctl start myapp
journalctl -u myapp -n 10

# 6. Cambiar env y reiniciar
sudo sed -i 's/production/development/' /etc/myapp/env
sudo systemctl restart myapp
journalctl -u myapp -n 5

# 7. Limpiar
sudo systemctl stop myapp
sudo systemctl disable myapp
sudo rm /etc/systemd/system/myapp.service
sudo rm -rf /opt/myapp /etc/myapp
sudo userdel myapp 2>/dev/null || true
sudo systemctl daemon-reload
```

### Ejercicio 3 — Oneshot service

```bash
# 1. Crear script
sudo tee /usr/local/bin/init-log.sh << 'EOF'
#!/bin/bash
echo "$(date): Initializing log file" >> /var/log/myapp-init.log
echo "Initialization complete" >> /var/log/myapp-init.log
EOF
sudo chmod +x /usr/local/bin/init-log.sh

# 2. Crear unidad oneshot
sudo tee /etc/systemd/system/init-log.service << 'EOF'
[Unit]
Description=Initialize Application Log
After=network.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/init-log.sh
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

# 3. Probar
sudo systemctl daemon-reload
sudo systemctl start init-log.service
systemctl status init-log.service

# 4. Verificar que quedó active
cat /var/log/myapp-init.log 2>/dev/null || echo "File not found"

# 5. Limpiar
sudo systemctl stop init-log.service 2>/dev/null || true
sudo systemctl disable init-log.service 2>/dev/null || true
sudo rm /etc/systemd/system/init-log.service
sudo rm /usr/local/bin/init-log.sh
sudo rm /var/log/myapp-init.log 2>/dev/null || true
sudo systemctl daemon-reload
```

### Ejercicio 4 — Identificar Type de servicios existentes

```bash
# Para cada servicio del sistema, identificar qué Type usa

for svc in sshd nginx cron docker postgresql httpd; do
    TYPE=$(systemctl show $svc --property=Type --value 2>/dev/null)
    PIDFILE=$(systemctl show $svc --property=PIDFile --value 2>/dev/null)
    if [[ -n "$TYPE" ]]; then
        echo "$svc: Type=$TYPE PIDFile=$PIDFILE"
    fi
done

# ¿Por qué nginx usa forking? Porque el master hace fork de workers
# ¿Por qué sshd usa forking? Porque hace fork por conexión (en some configs)
```

### Ejercicio 5 — Hardening de un servicio

```bash
# 1. Crear un servicio baseline
sudo tee /etc/systemd/system/secure-app.service << 'EOF'
[Unit]
Description=Secure Application

[Service]
Type=simple
ExecStart=/bin/sleep 60
User=root

[Install]
WantedBy=multi-user.target
EOF

# 2. Ver el nivel de seguridad actual
systemd-analyze security /etc/systemd/system/secure-app.service 2>/dev/null || echo "systemd-analyze not available"

# 3. Agregar hardening
sudo tee /etc/systemd/system/secure-app.service << 'EOF'
[Unit]
Description=Secure Application

[Service]
Type=simple
ExecStart=/bin/sleep 60
User=nobody
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=yes
PrivateTmp=true
PrivateDevices=true
ReadWritePaths=/var/log/secure-app
CapabilityBoundingSet=

[Install]
WantedBy=multi-user.target
EOF

# 4. Comparar el nivel de seguridad
systemd-analyze security /etc/systemd/system/secure-app.service 2>/dev/null

# 5. Limpiar
sudo rm /etc/systemd/system/secure-app.service
sudo systemctl daemon-reload
```
