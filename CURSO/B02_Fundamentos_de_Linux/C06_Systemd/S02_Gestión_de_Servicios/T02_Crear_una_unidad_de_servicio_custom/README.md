# T02 — Crear una unidad de servicio custom

## Anatomía completa de un .service

```ini
[Unit]
Description=My Application Server
Documentation=https://example.com/docs man:myapp(8)
After=network.target postgresql.service
Wants=postgresql.service

[Service]
Type=simple
User=myapp
Group=myapp
WorkingDirectory=/opt/myapp
ExecStart=/opt/myapp/bin/server --config /etc/myapp/config.yaml
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Las tres secciones son:
- **[Unit]** — metadata y dependencias (común a todos los tipos de unidad)
- **[Service]** — configuración específica del servicio
- **[Install]** — cómo se integra al boot

## Sección [Unit] — Detalle

```ini
[Unit]
# Descripción legible (aparece en systemctl status y logs):
Description=My Application Server

# Documentación (URLs, man pages):
Documentation=https://example.com/docs
Documentation=man:myapp(8)

# Dependencias de ordenamiento (no de activación):
After=network.target postgresql.service
# Arrancar DESPUÉS de network.target y postgresql.service
# After NO arranca esas unidades — solo define ORDEN si ya están activas

Before=nginx.service
# Arrancar ANTES de nginx.service

# Dependencias de activación:
Wants=postgresql.service
# Intenta arrancar postgresql, pero SI FALLA, myapp arranca igual

Requires=postgresql.service
# Arranca postgresql, y SI FALLA, myapp NO arranca

# Conflictos:
Conflicts=apache2.service
# Si apache2 está corriendo, se detiene al arrancar esta unidad
# Si esta unidad está corriendo, se detiene al arrancar apache2

# Condiciones (no arranca si no se cumplen, sin error):
ConditionPathExists=/etc/myapp/config.yaml
ConditionFileNotEmpty=/etc/myapp/config.yaml

# Assertions (no arranca si no se cumplen, CON error):
AssertPathExists=/opt/myapp/bin/server
```

## Sección [Service] — Detalle

### Type — Cómo systemd detecta que el servicio arrancó

```ini
# simple (default) — el proceso de ExecStart ES el servicio
[Service]
Type=simple
ExecStart=/opt/myapp/bin/server
# systemctl start retorna inmediatamente
# systemd asume que el servicio está listo cuando ExecStart se ejecuta

# exec — como simple, pero espera a que exec() tenga éxito
[Service]
Type=exec
ExecStart=/opt/myapp/bin/server
# systemctl start espera a que el binario se cargue en memoria
# Más seguro que simple (detecta errores de binario no encontrado)
# Disponible en systemd 240+

# forking — el proceso hace fork y el padre sale
[Service]
Type=forking
ExecStart=/opt/myapp/bin/server --daemon
PIDFile=/run/myapp.pid
# systemd espera a que el proceso padre salga
# El hijo se convierte en el proceso principal
# Necesita PIDFile para que systemd sepa cuál es el proceso principal
# EVITAR si es posible — es el modelo legacy

# notify — el proceso notifica a systemd cuando está listo
[Service]
Type=notify
ExecStart=/opt/myapp/bin/server
# El proceso envía sd_notify(READY=1) cuando está listo
# systemctl start espera a esa notificación
# El servicio debe usar la API de sd_notify (libsystemd)

# oneshot — para tareas que ejecutan y terminan
[Service]
Type=oneshot
ExecStart=/opt/myapp/bin/setup
RemainAfterExit=yes
# systemd espera a que ExecStart termine
# RemainAfterExit=yes: la unidad queda "active (exited)" después
# Útil para tareas de inicialización

# idle — como simple, pero espera a que todos los jobs terminen
[Service]
Type=idle
ExecStart=/opt/myapp/bin/server
# Retrasa la ejecución hasta que no hay otros jobs pendientes
# Útil para evitar mezclar salida con mensajes de boot
```

### Comandos de ejecución

```ini
[Service]
# Comando principal:
ExecStart=/opt/myapp/bin/server --config /etc/myapp/config.yaml
# DEBE ser una ruta absoluta (no se busca en PATH)

# Antes de arrancar:
ExecStartPre=/opt/myapp/bin/check-config
ExecStartPre=/usr/bin/mkdir -p /run/myapp
# Se ejecutan en orden, si uno falla el servicio no arranca
# Pueden ser múltiples

# Después de arrancar:
ExecStartPost=/opt/myapp/bin/notify-ready
# Se ejecuta después de que ExecStart tiene éxito

# Al hacer reload:
ExecReload=/bin/kill -HUP $MAINPID
# $MAINPID es el PID del proceso principal
# O:
ExecReload=/opt/myapp/bin/server --reload

# Al detener:
ExecStop=/opt/myapp/bin/server --stop
# Si no se define, systemd envía SIGTERM + SIGKILL (después del timeout)

# Después de detenerse:
ExecStopPost=/usr/bin/rm -f /run/myapp/socket
```

```ini
# Prefijos especiales en Exec*:
ExecStart=-/opt/myapp/bin/server
# - = ignorar el exit code (no marcar como failed si falla)

ExecStart=+/opt/myapp/bin/server
# + = ejecutar con privilegios completos (ignora User/Group)

ExecStart=!/opt/myapp/bin/server
# ! = ejecutar sin privilegios elevados del ambient capabilities

ExecStartPre=:/opt/myapp/bin/check
# : = no aplicar las restricciones de seguridad del servicio
```

### Usuario, grupo y permisos

```ini
[Service]
User=myapp
Group=myapp

# Crear usuario de sistema si no existe (systemd 235+):
DynamicUser=yes
# Crea un usuario efímero para la ejecución
# Se elimina cuando el servicio para
# Las rutas de datos deben usar StateDirectory/CacheDirectory

# Directorios gestionados:
StateDirectory=myapp           # /var/lib/myapp (con ownership correcto)
CacheDirectory=myapp           # /var/cache/myapp
LogsDirectory=myapp            # /var/log/myapp
ConfigurationDirectory=myapp   # /etc/myapp (read-only para el servicio)
RuntimeDirectory=myapp         # /run/myapp (se elimina al parar)

# Estos directorios se crean automáticamente con los permisos correctos
```

### Variables de entorno

```ini
[Service]
# Variables directas:
Environment=NODE_ENV=production
Environment="PORT=3000" "HOST=0.0.0.0"
# Las comillas permiten espacios en el valor

# Desde un archivo (una variable por línea, formato KEY=VALUE):
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local
# El - hace que sea opcional (no falla si no existe)

# Pasar variables del entorno del manager:
PassEnvironment=LANG TZ
```

```bash
# Archivo de entorno (/etc/myapp/env):
DATABASE_URL=postgresql://localhost/myapp
SECRET_KEY=abc123
LOG_LEVEL=info
# SIN export, SIN comillas (a menos que el valor las contenga)
# Líneas con # son comentarios
```

### Directorio de trabajo y entorno

```ini
[Service]
WorkingDirectory=/opt/myapp
# Directorio de trabajo del proceso

RootDirectory=/opt/myapp-chroot
# Ejecutar en un chroot

StandardOutput=journal
StandardError=journal
# Enviar stdout/stderr al journal (default)
# Alternativas: null, file:/path, append:/path, socket

SyslogIdentifier=myapp
# Nombre que aparece en los logs del journal

UMask=0077
# umask del proceso (default: 0022)
```

## Sección [Install] — Detalle

```ini
[Install]
# En qué target se activa al hacer enable:
WantedBy=multi-user.target
# Crea symlink en multi-user.target.wants/

# Dependencia fuerte:
RequiredBy=graphical.target
# Crea symlink en graphical.target.requires/

# Unidades adicionales a habilitar/deshabilitar juntas:
Also=myapp-worker.service myapp.socket

# Alias (nombres alternativos):
Alias=webapp.service
# systemctl enable myapp.service también crea webapp.service
```

## Ejemplos completos

### Aplicación Node.js

```ini
[Unit]
Description=Node.js Application
After=network.target

[Service]
Type=simple
User=nodeapp
Group=nodeapp
WorkingDirectory=/opt/nodeapp
ExecStart=/usr/bin/node /opt/nodeapp/server.js
Restart=on-failure
RestartSec=5
Environment=NODE_ENV=production PORT=3000
StandardOutput=journal
StandardError=journal
SyslogIdentifier=nodeapp

[Install]
WantedBy=multi-user.target
```

### Script de mantenimiento (oneshot)

```ini
[Unit]
Description=Database Cleanup
After=postgresql.service
Requires=postgresql.service

[Service]
Type=oneshot
User=postgres
ExecStart=/usr/local/bin/db-cleanup.sh
StandardOutput=journal

# No necesita [Install] si se activa solo desde un timer
```

### Aplicación con pre-checks

```ini
[Unit]
Description=My Application
After=network-online.target
Wants=network-online.target

[Service]
Type=notify
User=myapp
Group=myapp

ExecStartPre=/opt/myapp/bin/check-config /etc/myapp/config.yaml
ExecStartPre=/opt/myapp/bin/check-db-connection
ExecStart=/opt/myapp/bin/server
ExecReload=/bin/kill -HUP $MAINPID

Restart=on-failure
RestartSec=10
WatchdogSec=30

StateDirectory=myapp
RuntimeDirectory=myapp
EnvironmentFile=/etc/myapp/env
EnvironmentFile=-/etc/myapp/env.local

LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

## Procedimiento para crear un servicio

```bash
# 1. Crear el archivo de unidad:
sudo vim /etc/systemd/system/myapp.service

# 2. Recargar systemd:
sudo systemctl daemon-reload

# 3. Arrancar el servicio:
sudo systemctl start myapp.service

# 4. Verificar el estado:
systemctl status myapp.service

# 5. Ver los logs si algo falla:
journalctl -u myapp.service --no-pager -n 30

# 6. Habilitar al boot (si funciona):
sudo systemctl enable myapp.service

# 7. Verificar la unidad:
systemd-analyze verify /etc/systemd/system/myapp.service
```

## Seguridad — Hardening

```ini
[Service]
# Restringir el sistema de archivos:
ProtectSystem=strict          # /usr y /boot read-only, /etc read-only
ProtectHome=yes               # /home, /root, /run/user inaccesibles
PrivateTmp=yes                # /tmp aislado para este servicio
ReadWritePaths=/var/lib/myapp  # excepciones explícitas

# Restringir capacidades:
NoNewPrivileges=yes           # no puede ganar nuevos privilegios
CapabilityBoundingSet=        # sin capabilities extra

# Restringir red:
PrivateNetwork=yes            # sin acceso a la red (para tareas locales)
# O más granular:
RestrictAddressFamilies=AF_INET AF_INET6 AF_UNIX

# Restringir syscalls:
SystemCallFilter=@system-service  # solo syscalls típicas de servicios
SystemCallArchitectures=native    # solo la arquitectura actual

# Restringir dispositivos:
PrivateDevices=yes            # sin acceso a /dev (excepto /dev/null etc.)
DevicePolicy=closed           # no puede crear device nodes
```

```bash
# Ver el nivel de seguridad de un servicio:
systemd-analyze security myapp.service
# OVERALL EXPOSURE LEVEL: 4.2 MEDIUM
# Muestra cada directiva y su impacto
```

---

## Ejercicios

### Ejercicio 1 — Servicio simple

```bash
# Crear un script:
sudo tee /usr/local/bin/hello-service.sh << 'EOF'
#!/bin/bash
while true; do
    echo "$(date): Hello from service"
    sleep 10
done
EOF
sudo chmod +x /usr/local/bin/hello-service.sh

# Crear la unidad:
sudo tee /etc/systemd/system/hello.service << 'EOF'
[Unit]
Description=Hello World Service

[Service]
Type=simple
ExecStart=/usr/local/bin/hello-service.sh
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

# Probar:
sudo systemctl daemon-reload
sudo systemctl start hello
systemctl status hello
journalctl -u hello -f    # ver logs en vivo

# Limpiar:
sudo systemctl stop hello
sudo systemctl disable hello
sudo rm /etc/systemd/system/hello.service /usr/local/bin/hello-service.sh
sudo systemctl daemon-reload
```

### Ejercicio 2 — Inspeccionar seguridad

```bash
# ¿Qué nivel de seguridad tiene sshd?
systemd-analyze security sshd.service 2>/dev/null | tail -1
```

### Ejercicio 3 — Identificar el Type

```bash
# Para cada servicio, ¿qué Type usa?
for svc in sshd nginx cron; do
    TYPE=$(systemctl show "$svc" --property=Type --value 2>/dev/null)
    [[ -n "$TYPE" ]] && echo "$svc: Type=$TYPE"
done
```
