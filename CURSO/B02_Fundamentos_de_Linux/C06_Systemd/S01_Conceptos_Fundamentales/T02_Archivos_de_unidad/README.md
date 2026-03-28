# T02 — Archivos de unidad

## Ubicaciones

Los archivos de unidad pueden existir en tres directorios, cada uno con un
propósito y una precedencia diferente:

```bash
# 1. /usr/lib/systemd/system/ — Unidades instaladas por paquetes
#    Mantenidas por el gestor de paquetes (apt, dnf)
#    NO editar directamente — se sobrescriben al actualizar el paquete
ls /usr/lib/systemd/system/nginx.service
ls /usr/lib/systemd/system/sshd.service

# 2. /etc/systemd/system/ — Unidades y overrides del administrador
#    Creadas/editadas por el administrador
#    MÁXIMA precedencia — sobrescriben /usr/lib/systemd/system/
ls /etc/systemd/system/

# 3. /run/systemd/system/ — Unidades de runtime (temporales)
#    Generadas en runtime (ej: por generators)
#    Se pierden al reiniciar
#    Precedencia intermedia
ls /run/systemd/system/
```

### Precedencia

```
/etc/systemd/system/       ← MÁXIMA (admin)
      ↓ sobrescribe
/run/systemd/system/       ← intermedia (runtime)
      ↓ sobrescribe
/usr/lib/systemd/system/   ← MÍNIMA (paquetes)
```

```bash
# Si existe el mismo archivo en /etc/ y /usr/lib/:
# se usa el de /etc/ (el admin gana sobre el paquete)

# Verificar de dónde viene una unidad:
systemctl show nginx.service --property=FragmentPath
# FragmentPath=/usr/lib/systemd/system/nginx.service

# Si hiciste un override:
# FragmentPath=/etc/systemd/system/nginx.service
```

### Diferencia Debian vs RHEL

```bash
# Debian/Ubuntu también usa:
/lib/systemd/system/
# que es un symlink a /usr/lib/systemd/system/ en sistemas con /usr merge
# En sistemas sin merge, /lib/ es la ubicación real

ls -la /lib/systemd/system    # Debian
# → /usr/lib/systemd/system  (con /usr merge)

# RHEL siempre usa /usr/lib/systemd/system/
```

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

### Sección [Unit]

```ini
[Unit]
# Descripción (visible en systemctl status):
Description=My Custom Application Server

# Documentación (URL):
Documentation=https://example.com/docs
Documentation=man:myapp(8)

# Dependencias (ver T03 de S02 para detalle):
After=network.target postgresql.service
Requires=postgresql.service
Wants=redis.service

# Condiciones (la unidad no arranca si no se cumplen):
ConditionPathExists=/etc/myapp/config.conf
ConditionFileNotEmpty=/etc/myapp/config.conf
ConditionUser=root

# Asserts (como Condition, pero falla ruidosamente si no se cumple):
AssertPathExists=/usr/local/bin/myapp
```

### Sección [Service]

```ini
[Service]
# Tipo de servicio (cómo systemd determina que el servicio arrancó):
Type=simple              # el proceso de ExecStart ES el servicio (default)

# Comandos:
ExecStartPre=/usr/local/bin/myapp-check    # antes de arrancar
ExecStart=/usr/local/bin/myapp --config /etc/myapp/config.conf
ExecStartPost=/usr/local/bin/myapp-notify   # después de arrancar
ExecReload=/bin/kill -HUP $MAINPID          # al hacer reload
ExecStop=/usr/local/bin/myapp --stop         # al hacer stop (default: SIGTERM)

# Usuario y grupo:
User=myapp
Group=myapp

# Directorio de trabajo:
WorkingDirectory=/var/lib/myapp

# Variables de entorno:
Environment=NODE_ENV=production
Environment=PORT=3000
EnvironmentFile=/etc/myapp/env      # leer variables de un archivo

# Restart:
Restart=on-failure
RestartSec=5

# Límites de recursos:
LimitNOFILE=65535
MemoryMax=512M
CPUQuota=80%
```

### Sección [Install]

```ini
[Install]
# Determina qué pasa al hacer systemctl enable:

# WantedBy — el target que "quiere" esta unidad:
WantedBy=multi-user.target
# enable crea un symlink en /etc/systemd/system/multi-user.target.wants/

# RequiredBy — el target que "requiere" esta unidad:
RequiredBy=graphical.target

# Also — unidades adicionales a habilitar/deshabilitar juntas:
Also=myapp.socket myapp-worker.service

# Alias — nombres alternativos:
Alias=webapp.service
```

```bash
# Lo que hace systemctl enable:
systemctl enable nginx.service
# Crea: /etc/systemd/system/multi-user.target.wants/nginx.service
#        → /usr/lib/systemd/system/nginx.service (symlink)

# Lo que hace systemctl disable:
systemctl disable nginx.service
# Elimina el symlink

# Ver los symlinks:
ls -la /etc/systemd/system/multi-user.target.wants/
```

## Editar unidades — systemctl edit

### Override parcial (drop-in)

```bash
# systemctl edit crea un "drop-in" que modifica la unidad original
# sin reemplazarla:
sudo systemctl edit nginx.service
# Abre un editor con un archivo vacío
# Se guarda en: /etc/systemd/system/nginx.service.d/override.conf
```

```ini
# override.conf — solo las directivas que quieres cambiar:
[Service]
MemoryMax=1G
Environment=WORKER_PROCESSES=4
```

```bash
# La unidad resultante combina:
# 1. /usr/lib/systemd/system/nginx.service  (original del paquete)
# 2. /etc/systemd/system/nginx.service.d/override.conf  (tu override)

# Ver la configuración efectiva (merged):
systemctl cat nginx.service
# Muestra el archivo original + overrides

# Ver todos los drop-ins:
systemd-delta
# Muestra qué unidades tienen overrides y de qué tipo
```

### Override completo

```bash
# systemctl edit --full crea una copia completa en /etc/:
sudo systemctl edit --full nginx.service
# Copia /usr/lib/systemd/system/nginx.service
# a /etc/systemd/system/nginx.service
# y abre el editor

# CUIDADO: al hacer override completo, las actualizaciones
# del paquete NO se reflejan — tu copia en /etc/ tiene precedencia
```

### Revertir cambios

```bash
# Eliminar un drop-in:
sudo rm -rf /etc/systemd/system/nginx.service.d/
sudo systemctl daemon-reload

# Revertir un override completo:
sudo rm /etc/systemd/system/nginx.service
sudo systemctl daemon-reload

# systemctl revert (systemd 235+):
sudo systemctl revert nginx.service
# Elimina todos los overrides y drop-ins
```

## daemon-reload

Cada vez que se modifica un archivo de unidad, hay que notificar a systemd:

```bash
sudo systemctl daemon-reload
# Re-lee todos los archivos de unidad
# NO reinicia servicios — solo recarga la configuración

# Después de daemon-reload, reiniciar el servicio para que los cambios
# tomen efecto:
sudo systemctl daemon-reload
sudo systemctl restart nginx.service
```

```bash
# Si olvidas daemon-reload, systemctl te avisa:
# Warning: nginx.service changed on disk. Run 'systemctl daemon-reload'
# to reload units.
```

## Drop-in directories

Además de `systemctl edit`, se pueden crear drop-ins manualmente:

```bash
# Crear el directorio:
sudo mkdir -p /etc/systemd/system/nginx.service.d/

# Crear archivos .conf (se aplican en orden alfabético):
sudo cat > /etc/systemd/system/nginx.service.d/10-limits.conf << 'EOF'
[Service]
LimitNOFILE=65535
EOF

sudo cat > /etc/systemd/system/nginx.service.d/20-restart.conf << 'EOF'
[Service]
Restart=always
RestartSec=5
EOF

sudo systemctl daemon-reload
```

```bash
# Orden de aplicación:
# 1. Unidad original
# 2. Drop-ins en orden alfanumérico (10-limits.conf, 20-restart.conf)
# Los valores se sobrescriben; las listas se acumulan (depende de la directiva)

# Para LIMPIAR una directiva de lista antes de redefinirla, asignar vacío:
[Service]
ExecStart=                            # limpia el ExecStart original
ExecStart=/usr/local/bin/myapp-v2     # nuevo valor
```

## Generators

Los generators son programas que generan unidades dinámicamente durante el
boot:

```bash
# Ubicaciones de generators:
/usr/lib/systemd/system-generators/
/etc/systemd/system-generators/
/run/systemd/system-generators/

# Generators comunes:
systemd-fstab-generator     # genera .mount desde /etc/fstab
systemd-cryptsetup-generator # genera unidades para LUKS desde /etc/crypttab
systemd-gpt-auto-generator   # genera .mount desde particiones GPT

# Los generators se ejecutan al boot y al hacer daemon-reload
# Su salida va a /run/systemd/generator/

ls /run/systemd/generator/
```

## Inspeccionar unidades

```bash
# Ver el archivo de unidad completo (con overrides):
systemctl cat sshd.service

# Ver todas las propiedades:
systemctl show sshd.service

# Ver propiedades específicas:
systemctl show sshd.service --property=MainPID,ActiveState,SubState

# Ver qué archivo se usa:
systemctl show sshd.service --property=FragmentPath

# Ver drop-ins aplicados:
systemctl show sshd.service --property=DropInPaths

# Verificar sintaxis (sin aplicar):
systemd-analyze verify /etc/systemd/system/myapp.service

# Ver todas las unidades que difieren del paquete:
systemd-delta
# [EXTENDED]   /etc/systemd/system/docker.service.d/override.conf
# [OVERRIDDEN] /etc/systemd/system/nginx.service
```

---

## Ejercicios

### Ejercicio 1 — Explorar ubicaciones

```bash
# ¿Cuántas unidades hay en cada directorio?
echo "Paquetes:"; ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo "Admin:"; ls /etc/systemd/system/*.service 2>/dev/null | wc -l
echo "Runtime:"; ls /run/systemd/system/*.service 2>/dev/null | wc -l

# ¿Hay overrides (drop-ins)?
find /etc/systemd/system -name "*.d" -type d 2>/dev/null
```

### Ejercicio 2 — Leer una unidad

```bash
# Ver la unidad completa de sshd (o cualquier servicio activo):
systemctl cat sshd.service

# Identificar:
# - ¿En qué directorio está?
# - ¿Qué Type= usa?
# - ¿De qué target depende?
# - ¿Tiene drop-ins?
```

### Ejercicio 3 — Verificar cambios

```bash
# Ver si alguna unidad tiene overrides:
systemd-delta --type=extended,overridden 2>/dev/null

# Ver la configuración efectiva vs la original:
# systemctl cat docker.service  (si tiene overrides)
```
