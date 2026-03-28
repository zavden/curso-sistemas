# T03 — Dependencias

## Los dos ejes de las dependencias

Systemd maneja las dependencias en **dos dimensiones independientes**:

```
┌─────────────────────────────────────────────────────────────┐
│                  DEPENDENCIAS DE ACTIVACIÓN                  │
│                   "¿Arrancas la otra unidad?"                 │
│                                                              │
│   Wants=        → Quisiera que arranques (débil)           │
│   Requires=     → Necesito que arranques (fuerte)          │
│   BindsTo=     → Fuertísimo (muere conmigo)               │
│   Requisite=   → Ya debieras estar corriendo               │
│   PartOf=      → Si me detenés, te detenés                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                  DEPENDENCIAS DE ORDEN                      │
│                  "¿En qué orden arrancamos?"                │
│                                                              │
│   After=         → Arrancás DESPUÉS de estas unidades      │
│   Before=        → Arrancás ANTES de estas unidades        │
└─────────────────────────────────────────────────────────────┘

         ⚠️ Son INDEPENDIENTES ⚠️

   Wants= sin After=  → arrancan EN PARALELO
   After= sin Wants= → define orden pero no arranca nada
```

```ini
# Patron comun ( Wants= + After= ):
[Unit]
Wants=postgresql.service      # intentamos arrancar postgresql
After=postgresql.service      # esperamos a que esté listo

# Requiere fuerte ( Requires= + After= ):
[Unit]
Requires=postgresql.service   # postgresql debe arrancar
After=postgresql.service      # esperamos a que esté listo

# Solo orden (After= solo):
[Unit]
After=network.target          # si network.target está activo,
                             # arrancamos después

# Solo activación (Wants= solo):
[Unit]
Wants=redis.service          # intentamos arrancar redis
# Arrancan EN PARALELO
```

## Dependencias de activación

### Wants= — Dependencia débil

```
┌─────────────────────────────────────────────────────────────┐
│                         Wants=                                │
│                                                              │
│   Wants=redis.service                                       │
│                                                              │
│   1. Intenta arrancar redis.service                         │
│   2. Si redis FALLA → esta unidad ARRANCA IGUAL          │
│   3. Si redis se DETIENE después → esta unidad SIGUE    │
│                                                              │
│   Es la dependencia MAS USADA                               │
│   Tolerante a fallos — no rompe el boot si algo falla     │
└─────────────────────────────────────────────────────────────┘
```

```ini
# Ejemplo: app que usa redis como cache (opcional)
[Unit]
Description=My Application
Wants=redis.service
After=redis.service

[Service]
ExecStart=/opt/myapp/bin/server
Restart=on-failure
```

```bash
# Wants= también se crea dinámicamente:
# Los symlinks en target.wants/ son Wants= implícitos
ls /etc/systemd/system/multi-user.target.wants/
# → Cada symlink = Wants= de esa unidad
```

### Requires= — Dependencia fuerte

```
┌─────────────────────────────────────────────────────────────┐
│                        Requires=                             │
│                                                              │
│   Requires=postgresql.service                               │
│                                                              │
│   1. Arranca postgresql.service                            │
│   2. Si postgresql FALLA → esta unidad NO ARRANCA        │
│   3. Si postgresql se DETIENE después →                    │
│      esta unidad también se DETIENE                        │
│                                                              │
│   Usar SOLO cuando la dependencia es CRÍTICA             │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Unit]
Description=Database-Backed Application
Requires=postgresql.service
After=postgresql.service

[Service]
ExecStart=/opt/myapp/bin/server
# Si postgresql falla, este servicio NO arranca
```

### BindsTo= — Dependencia máxima

```
┌─────────────────────────────────────────────────────────────┐
│                        BindsTo=                             │
│                                                              │
│   Como Requires=, pero MÁS ESTRICTO:                        │
│                                                              │
│   Requires= → si la dependencia se detiene,               │
│               esta unidad SE DETIENE                      │
│                                                              │
│   BindsTo=  → si la dependencia entra en CUALQUIER        │
│               estado INACTIVO, esta unidad se DETIENE     │
│                                                              │
│   Útil para: dispositivos .device, mounts .mount         │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Unit]
Description=RAID Monitoring
BindsTo=mdraid.service
After=mdraid.service

# Si mdraid.service entra en failed/inactive → nos detenemos
```

### Requisite= — Ya debieras estar corriendo

```
┌─────────────────────────────────────────────────────────────┐
│                       Requisite=                             │
│                                                              │
│   Requisite=nginx.service                                  │
│                                                              │
│   1. NO intenta arrancar nginx.service                    │
│   2. Si nginx YA está activo → esta unidad arranca       │
│   3. Si nginx NO está activo → esta unidad FALLA          │
│                                                              │
│   Muy estricto — para cosas que deben existir              │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Unit]
Description=Configuration Reloader
Requisite=nginx.service
After=nginx.service

# Si nginx no está corriendo cuando intentamos arrancar →
# esta unidad falla inmediatamente
```

### PartOf= — Propagación de stop/restart

```
┌─────────────────────────────────────────────────────────────┐
│                        PartOf=                               │
│                                                              │
│   PartOf=myapp.service                                     │
│                                                              │
│   Si myapp.service se DETIENE o REINICIA →                │
│   esta unidad también se DETIENE o REINICIA               │
│                                                              │
│   NO propaga el START                                       │
│                                                              │
│   Ideal para: workers, sidecars, componentes auxiliares     │
└─────────────────────────────────────────────────────────────┘
```

```ini
# Ejemplo: app principal + worker + logger
# myapp.service
[Unit]
Description=My Application
Wants=myapp-worker.service myapp-logger.service

# myapp-worker.service
[Unit]
Description=My App Background Worker
PartOf=myapp.service
After=myapp.service

# myapp-logger.service
[Unit]
Description=My App Logger
PartOf=myapp.service
After=myapp.service

# systemctl restart myapp:
# 1. myapp se detiene
# 2. myapp-worker se detiene automáticamente (PartOf)
# 3. myapp-logger se detiene automáticamente (PartOf)
# 4. myapp arranca
# 5. myapp-worker arranca
# 6. myapp-logger arranca
```

## Dependencias de ordenamiento

### After= y Before=

```ini
# After — arrancar DESPUÉS de:
[Unit]
After=network.target postgresql.service redis.service
# → network.target, postgresql y redis_arrancan primero
# → Esta unidad espera a que estén "active" antes de iniciar

# Before — arrancar ANTES de:
[Unit]
Before=gdm.service
# → gdm.service espera a que esta unidad arranque primero
# → Útil para asegurarsede quealgo está listo ANTES de un servicio
```

```
┌─────────────────────────────────────────────────────────────┐
│  After=/Before= NO ACTIVAN nada — solo definen ORDEN       │
│                                                              │
│   After=network.target                                       │
│   → SI network.target está activo: esperamos               │
│   → SI network.target NO está activo: After se IGNORA      │
│                                                              │
│   Por eso se combina con Wants=/Requires:                   │
│   Wants=network.target  → intentamos arrancar               │
│   After=network.target → esperamos a que esté activo      │
└─────────────────────────────────────────────────────────────┘
```

### Sin ordenamiento — Arranque en paralelo

```ini
# Sin After=/Before=, las unidades arrancan EN PARALELO:
[Unit]
Wants=redis.service
Wants=memcached.service
Wants=elasticsearch.service

# Las tres arrancan SIMULTANEAMENTE
# → Boot más rápido (paralelismo)
# → Bien para servicios independientes
```

## Conflicts= — Incompatibilidad

```ini
[Unit]
Conflicts=apache2.service
# → Si apache2 está corriendo, nos detenemos
# → Si nosotros estamos corriendo, apache2 se detiene

# Caso real: firewalld vs iptables
# firewalld.service:
[Unit]
Conflicts=iptables.service ip6tables.service ebtables.service
```

## Resumen de directivas de dependencia

| Directiva | Activa la dependencia? | Si la dependencia falla | Si la dependencia se detiene después |
|---|---|---|---|
| `Wants=` | ✅ Intenta | ⚠️ Arranca igual | 🟡 Sigue corriendo |
| `Requires=` | ✅ Sí | ❌ No arranca | 🔴 Se detiene |
| `BindsTo=` | ✅ Sí | ❌ No arranca | 🔴 Se detiene (cualquier estado inactivo) |
| `Requisite=` | ❌ No | ❌ Falla inmediatamente | 🔴 Se detiene |
| `PartOf=` | ❌ No | — | 🔴 Se detiene/restarta con la otra |
| `After=` | ❌ No | — | — (solo ordena) |
| `Before=` | ❌ No | — | — (solo ordena) |

## Targets como puntos de sincronización

Los targets son grupos de unidades y puntos de sincronización en el boot:

```
boot timeline:

systemd (PID 1)
    │
    ▼
sysinit.target         ← inicialización básica (locks, kernel, mounts)
    │
    ▼
basic.target           ← servicios básicos (DBus, sockets, timers)
    │
    ▼
    ├─►sockets.target    ← todos los sockets escuchando
    ├─►timers.target     ← todos los timers activos
    │
    ▼
network.target         ← red configurada (IP asignada)
    │
    ▼
network-online.target  ← red con conectividad REAL (ruta verificada)
    │
    ▼
multi-user.target     ← modo multiusuario (sin GUI)
    │
    ▼
graphical.target      ← modo gráfico (con GUI)
```

```bash
# Targets del sistema:
systemctl list-units --type=target --state=active
```

### network.target vs network-online.target

```
network.target:
  → Las interfaces tienen IP asignada
  → bind() a un puerto funciona
  → PERO puede no haber ruta a Internet
  → Suficiente para: servidores que ESCUCHAN (nginx, sshd)

network-online.target:
  → Hay conectividad verificada
  → La ruta por defecto está configurada
  → Suficiente para: servicios que CONECTAN a hosts remotos
  → Más lento — wait for route propagation
```

```ini
# Servidor que escucha (no necesita red completa):
[Unit]
Description=HTTP Server
After=network.target        # IP asignada = listo
# No necesita network-online.target

# Cliente que conecta a base de datos remota:
[Unit]
Description=My App
After=network-online.target
Wants=network-online.target
# Necesita la red REAL para conectar a la DB
```

## Targets importantes

```bash
sysinit.target         → servicios muy básicos (locks, mounts)
basic.target           → DBus, sockets, logging básico
sockets.target        → todos los sockets (paralelismo en boot)
timers.target         → todos los timers
local-fs.target       → sistemas de archivos locales montados
remote-fs.target      → sistemas de archivos remotos (NFS)
network.target        → interfaces con IP
network-online.target → conectividad real verificada
multi-user.target    → modo multiusuario (sin GUI)
graphical.target      → modo gráfico (GDM, SDDM)
```

## Visualizar dependencias

```bash
# Árbol de dependencias (hacia abajo = lo que NECESITA esta unidad):
systemctl list-dependencies nginx.service

# reverse (hacia arriba = lo que DEPENDE de esta unidad):
systemctl list-dependencies nginx.service --reverse

# Todas las dependencias (recursivo):
systemctl list-dependencies nginx.service --all

# Target por defecto y sus dependencias:
systemctl list-dependencies $(systemctl get-default)
```

```bash
# systemd-analyze: tiempos y cadenas críticas
systemd-analyze                      # tiempo total del boot
systemd-analyze blame               # servicios más lentos
systemd-analyze critical-chain      # cadena crítica

# Ejemplo de critical-chain:
systemd-analyze critical-chain multi-user.target
# multi-user.target @15.234s
# └─nginx.service @10.500s +4.734s
#   └─network-online.target @10.050s +0.450s
#     └─NetworkManager.service @3.000s +7.050s
```

```bash
# Generar imagen SVG del boot:
systemd-analyze plot > /tmp/boot.svg
# Ver en un navegador o convertir a PNG

# Ver el bottleneck del boot:
systemd-analyze blame | head -20
```

## DefaultDependencies — Dependencias implícitas

Por defecto, systemd agrega dependencias automáticamente:

```ini
[Service]
DefaultDependencies=yes   # es el default

# systemd agrega IMPLÍCITAMENTE:
# Requires=sysinit.target
# After=sysinit.target basic.target
# Conflicts=shutdown.target
# Before=shutdown.target

# Esto significa que todo servicio normal:
# → Espera a que sysinit.target complete
# → Se detiene antes del apagado
```

```bash
# Para unidades que necesitan arrancar MUY temprano (antes de sysinit):
[Unit]
DefaultDependencies=no
# CUIDADO: necesitas definir todas las dependencias manualmente
# Para: system managers early boot, initramfs services
```

## Errores comunes

### Loop de dependencias

```
# A Después B, B Después A → DEPENDENCY CYCLE
systemd[1]: myapp.service: Found ordering cycle:
#   myapp.service → postgresql.service → myapp.service
# systemd intentará romper el ciclo y seguirá
```

```bash
# Detectar ciclos:
systemctl list-dependencies myapp.service --all | grep -E "myapp|postgres"

# systemd-analyze verify reporta ciclos:
systemd-analyze verify myapp.service

# Solución: revisar los After/Before y romper el ciclo
```

### Requires= sin After=

```ini
# ERROR SUTIL: Requires sin After = arranque en paralelo
[Unit]
Requires=postgresql.service
# postgresql y esta unidad ARRANAN SIMULTANEAMENTE
# Esta unidad puede intentar conectar antes de que postgresql esté listo

# CORRECTO:
[Unit]
Requires=postgresql.service
After=postgresql.service
```

### network.target vs network-online.target

```ini
# ERROR: para servicios que conectan a hosts REMOTOS
[Unit]
After=network.target
# Las interfaces tienen IP, PERO puede no haber ruta a Internet
# Tu servicio intenta conectar a la base de datos → FALLA

# CORRECTO:
[Unit]
After=network-online.target
Wants=network-online.target
# Espera a que HAYA CONECTIVIDAD REAL
```

### Wants= y WantedBy= son cosas distintas

```
Wants= → en [Unit], define dependencia de activación
WantedBy= → en [Install], define cómo se habilita
```

```ini
[Unit]
Wants=redis.service         # ← dependencia
After=redis.service

[Install]
WantedBy=multi-user.target  # ← enable crea symlink en multi-user.target.wants/
```

---

## Ejercicios

### Ejercicio 1 — Explorar dependencias de un servicio

```bash
# 1. Ver de qué depende sshd
systemctl list-dependencies sshd.service

# 2. Ver qué depende de network.target (reverse)
systemctl list-dependencies network.target --reverse | head -20

# 3. Ver qué unidades son "wanted by" multi-user.target
systemctl list-dependencies multi-user.target --reverse | head -20

# 4. Ver propiedades de dependencias de sshd
systemctl show sshd.service --property=Requires,After,Wants,Conflicts
```

### Ejercicio 2 — Analizar el boot

```bash
# 1. Tiempo total del boot
systemd-analyze

# 2. Los 10 servicios más lentos
systemd-analyze blame --no-pager | head -10

# 3. Cadena crítica
systemd-analyze critical-chain --no-pager 2>&1 | head -20

# 4. Si tienes svg support, generar plot
systemd-analyze plot > /tmp/boot.svg
echo "Saved to /tmp/boot.svg"
```

### Ejercicio 3 — Crear servicio con dependencias

```bash
# 1. Crear un script simple
sudo tee /usr/local/bin/app-with-deps.sh << 'EOF'
#!/bin/bash
echo "$(date): app started" >> /var/log/app.log
sleep 5
echo "$(date): app is running" >> /var/log/app.log
EOF
sudo chmod +x /usr/local/bin/app-with-deps.sh

# 2. Crear un servicio con dependencias
sudo tee /etc/systemd/system/app-deps.service << 'EOF'
[Unit]
Description=App with Dependencies
After=network.target cron.service
Wants=cron.service

[Service]
Type=oneshot
ExecStart=/usr/local/bin/app-with-deps.sh
RemainAfterExit=no

[Install]
WantedBy=multi-user.target
EOF

# 3. Recargar y verificar
sudo systemctl daemon-reload
systemctl start app-deps.service
systemctl status app-deps.service

# 4. Ver logs
cat /var/log/app.log 2>/dev/null || echo "No log yet"

# 5. Limpiar
sudo systemctl stop app-deps.service
sudo rm /etc/systemd/system/app-deps.service
sudo rm /usr/local/bin/app-with-deps.sh
sudo rm /var/log/app.log
sudo systemctl daemon-reload
```

### Ejercicio 4 — PartOf para restart automático

```bash
# 1. Crear servicio principal
sudo tee /etc/systemd/system/myservice.service << 'EOF'
[Unit]
Description=Main Service
After=network.target

[Service]
Type=simple
ExecStart=/bin/sleep 60
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

# 2. Crear worker con PartOf
sudo tee /etc/systemd/system/myservice-worker.service << 'EOF'
[Unit]
Description=Worker for Main Service
PartOf=myservice.service
After=myservice.service

[Service]
Type=simple
ExecStart=/bin/sleep 60

[Install]
WantedBy=multi-user.target
EOF

# 3. Habilitar y arrancar
sudo systemctl daemon-reload
sudo systemctl enable --now myservice.service

# 4. Ver que el worker también está activo
systemctl status myservice-worker.service

# 5. Reiniciar el principal
sudo systemctl restart myservice.service
# El worker debería detenerse y reiniciarse automáticamente

# 6. Limpiar
sudo systemctl stop myservice.service myservice-worker.service
sudo systemctl disable myservice.service myservice-worker.service
sudo rm /etc/systemd/system/myservice.service \
        /etc/systemd/system/myservice-worker.service
sudo systemctl daemon-reload
```

### Ejercicio 5 — Diagnóstico de dependencias

```bash
# 1. Crear un servicio con Requires circulare (NO hacer esto en producción)
# Simular: servicio A requiere B, B requiere A

# A:
sudo tee /etc/systemd/system/svc-a.service << 'EOF'
[Unit]
Description=Service A
Requires=svc-b.service
After=svc-b.service
[Service]
Type=oneshot
ExecStart=/bin/echo "A started"
RemainAfterExit=yes
[Install]
WantedBy=multi-user.target
EOF

# B:
sudo tee /etc/systemd/system/svc-b.service << 'EOF'
[Unit]
Description=Service B
Requires=svc-a.service
After=svc-a.service
[Service]
Type=oneshot
ExecStart=/bin/echo "B started"
RemainAfterExit=yes
[Install]
WantedBy=multi-user.target
EOF

# 2. Intentar iniciar (ver el error)
sudo systemctl daemon-reload
sudo systemctl start svc-a.service 2>&1

# 3. Ver el mensaje de ciclo
journalctl -xe 2>&1 | grep -A5 "cycle\|ordering"

# 4. Limpiar
sudo rm /etc/systemd/system/svc-a.service \
        /etc/systemd/system/svc-b.service
sudo systemctl daemon-reload
```

### Ejercicio 6 — Comparar After vs WantedBy vs Requires

```bash
# Script que demuestra la diferencia

echo "=== After= (solo orden) ==="
echo "Unidad A: After=B"
echo "B no se arranca automáticamente"
echo ""

echo "=== Wants= (solo activación) ==="
echo "Unidad A: Wants=B"
echo "A y B arrancan en PARALELO"
echo ""

echo "=== Wants= + After= (patrón común) ==="
echo "Unidad A: Wants=B, After=B"
echo "B se intenta arrancar"
echo "A espera a que B esté activo"
echo ""

echo "=== Requires= + After= (dependencia fuerte) ==="
echo "Unidad A: Requires=B, After=B"
echo "B se arranca"
echo "A espera a que B esté activo"
echo "Si B falla → A no arranca"
```
