# Lab — Crear una unidad de servicio custom

## Objetivo

Dominar la creacion de servicios systemd: los diferentes Type=
(simple, oneshot, forking, notify), comandos de ejecucion con
prefijos especiales, configuracion de usuario/entorno, directorios
gestionados, hardening de seguridad, y validacion con
systemd-analyze verify/security.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Type y ExecStart

### Objetivo

Entender como Type= afecta el comportamiento del servicio y
crear unit files con diferentes tipos.

### Paso 1.1: Type=simple (default)

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
echo "  systemd asume que esta listo cuando ExecStart se ejecuta"
echo "  Es el Type por defecto (si no se especifica)"
'
```

### Paso 1.2: Type=oneshot

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
echo "  Util para tareas de inicializacion y scripts de setup"
echo "  Puede tener multiples ExecStart= (se ejecutan en orden)"
'
```

### Paso 1.3: Type=forking y Type=notify

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
echo "  PIDFile= indica donde encontrar el PID del hijo"
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
echo "notify: el proceso envia sd_notify(READY=1) cuando esta listo"
echo "  systemctl start espera la notificacion"
echo "  WatchdogSec= habilita health checks"
echo "  El servicio debe usar la API sd_notify (libsystemd)"
'
```

### Paso 1.4: ExecStart y prefijos

```bash
docker compose exec debian-dev bash -c '
echo "=== ExecStart y prefijos ==="
echo ""
echo "ExecStart DEBE usar ruta absoluta (no busca en PATH)"
echo ""
echo "--- Comandos disponibles ---"
echo "ExecStartPre=   antes de arrancar (multiples, en orden)"
echo "ExecStart=      comando principal"
echo "ExecStartPost=  despues de arrancar"
echo "ExecReload=     al hacer reload (tipicamente: kill -HUP \$MAINPID)"
echo "ExecStop=       al hacer stop (default: SIGTERM)"
echo "ExecStopPost=   despues de detenerse"
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

## Parte 2 — Entorno y directorios

### Objetivo

Configurar usuario, variables de entorno, y directorios gestionados.

### Paso 2.1: User, Group y WorkingDirectory

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
echo "User=/Group= definen con que permisos corre el proceso"
echo "WorkingDirectory= define el directorio de trabajo"
echo "UMask= define el umask (default: 0022)"
echo "SyslogIdentifier= nombre en los logs del journal"
'
```

### Paso 2.2: Variables de entorno

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
# Lineas con # son comentarios
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

### Paso 2.3: Directorios gestionados

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorios gestionados ==="
echo ""
echo "systemd puede crear directorios automaticamente"
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
echo "RuntimeDirectory se elimina automaticamente al parar el servicio"
'
```

---

## Parte 3 — Hardening y verificacion

### Objetivo

Aplicar directivas de seguridad y verificar el nivel de
proteccion con systemd-analyze security.

### Paso 3.1: Directivas de seguridad

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
echo "CapabilityBoundingSet=  sin capabilities extra (vacio = ninguna)"
'
```

### Paso 3.2: systemd-analyze security

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze security ==="
echo ""
echo "Evalua el nivel de seguridad de un servicio"
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
echo "La puntuacion va de 0.0 (mas seguro) a 10.0 (menos seguro)"
echo "OVERALL EXPOSURE LEVEL:"
echo "  0.0-2.0  SAFE"
echo "  2.0-4.0  OK"
echo "  4.0-7.0  MEDIUM"
echo "  7.0-10.0 UNSAFE"
'
```

### Paso 3.3: Ejemplo completo — servicio de produccion

```bash
docker compose exec debian-dev bash -c '
echo "=== Servicio de produccion completo ==="
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
echo "  - Restart con limites (on-failure, StartLimit)"
echo "  - Watchdog (WatchdogSec)"
echo "  - Timeouts (start y stop)"
echo "  - Directorios gestionados (State, Runtime)"
echo "  - Variables de entorno (EnvironmentFile)"
echo "  - Hardening (Protect*, NoNewPrivileges)"
'
```

---

## Limpieza final

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

---

## Conceptos reforzados

1. `Type=` define como systemd detecta que el servicio arranco:
   `simple` (el proceso ES el servicio), `oneshot` (ejecuta y
   termina), `forking` (legacy, hace fork), `notify` (envia
   sd_notify).

2. `ExecStart=` requiere ruta absoluta. Los prefijos `-` (ignorar
   exit code), `+` (privilegios completos) modifican el
   comportamiento. `ExecStartPre=` puede ser multiple.

3. `EnvironmentFile=` carga variables desde un archivo (formato
   `KEY=VALUE`, sin `export`). El prefijo `-` lo hace opcional.

4. Los directorios gestionados (`StateDirectory=`, `RuntimeDirectory=`)
   se crean automaticamente con ownership correcto.
   `RuntimeDirectory=` se elimina al parar el servicio.

5. Las directivas de hardening (`ProtectSystem=strict`,
   `ProtectHome=yes`, `PrivateTmp=yes`, `NoNewPrivileges=yes`)
   restringen el acceso del servicio al filesystem y privilegios.

6. `systemd-analyze verify` valida la sintaxis y `systemd-analyze
   security` evalua el nivel de seguridad (0.0=seguro, 10.0=inseguro).
