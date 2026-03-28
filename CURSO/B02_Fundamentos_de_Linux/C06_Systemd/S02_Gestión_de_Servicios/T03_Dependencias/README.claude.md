# T03 — Dependencias

## Erratas corregidas respecto a las fuentes originales

| # | Ubicación | Error | Corrección |
|---|-----------|-------|------------|
| 1 | max:16 | PartOf descrito como "Si me detenés, te detenés" — dirección invertida | PartOf significa: si **la otra** unidad se detiene/reinicia, **esta** se detiene/reinicia |
| 2 | max:213-216 | Ejemplo de restart con PartOf sugiere que PartOf propaga el start | PartOf NO propaga start; el re-arranque de workers lo causa `Wants=` en la unidad principal |
| 3 | max:227 | `redis_arrancan primero` — typo con guión bajo | `redis arrancan primero` |
| 4 | max:234 | `asegurarsede quealgo` — palabras pegadas | `asegurarse de que algo` |
| 5 | max:467 | `ARRANAN SIMULTANEAMENTE` — typo | `ARRANCAN SIMULTÁNEAMENTE` |

---

## Dos ejes independientes

Las dependencias en systemd se organizan en **dos dimensiones independientes**:

```
┌─────────────────────────────────────────────────────────────┐
│                  DEPENDENCIAS DE ACTIVACIÓN                 │
│                "¿Se arranca la otra unidad?"                 │
│                                                             │
│   Wants=        → Quisiera que arranques (débil)            │
│   Requires=     → Necesito que arranques (fuerte)           │
│   BindsTo=      → Fuertísimo (muero si te detenés)          │
│   Requisite=    → Ya debieras estar corriendo               │
│   PartOf=       → Si te detenés, me detengo                 │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                  DEPENDENCIAS DE ORDEN                      │
│                "¿En qué orden arrancamos?"                  │
│                                                             │
│   After=         → Arranco DESPUÉS de estas unidades        │
│   Before=        → Arranco ANTES de estas unidades          │
└─────────────────────────────────────────────────────────────┘

         ⚠️ Son INDEPENDIENTES ⚠️

   Wants= sin After=  → arrancan EN PARALELO
   After= sin Wants=  → define orden pero no arranca nada
```

```ini
# Patrón habitual — activación + orden:
[Unit]
Requires=postgresql.service    # arrancar postgresql
After=postgresql.service       # esperar a que esté listo

# Solo orden (postgresql lo arranca otro):
[Unit]
After=postgresql.service       # si postgresql está activo, arrancar después

# Solo activación (arrancar en paralelo):
[Unit]
Wants=redis.service            # arrancar redis, sin esperar orden
```

## Dependencias de activación

### Wants= — Dependencia débil

```
┌─────────────────────────────────────────────────────────────┐
│                         Wants=                              │
│                                                             │
│   Wants=redis.service                                       │
│                                                             │
│   1. Intenta arrancar redis.service                         │
│   2. Si redis FALLA → esta unidad ARRANCA IGUAL             │
│   3. Si redis se DETIENE después → esta unidad SIGUE        │
│                                                             │
│   Es la dependencia MÁS USADA                              │
│   Tolerante a fallos — no rompe el boot si algo falla       │
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

# Crear dependencias Wants sin editar la unidad:
sudo mkdir -p /etc/systemd/system/myapp.service.d/
echo -e "[Unit]\nWants=redis.service\nAfter=redis.service" | \
    sudo tee /etc/systemd/system/myapp.service.d/wants-redis.conf
```

### Requires= — Dependencia fuerte

```
┌─────────────────────────────────────────────────────────────┐
│                        Requires=                            │
│                                                             │
│   Requires=postgresql.service                               │
│                                                             │
│   1. Arranca postgresql.service                             │
│   2. Si postgresql FALLA → esta unidad NO ARRANCA           │
│   3. Si postgresql se DETIENE después →                     │
│      esta unidad también se DETIENE                         │
│                                                             │
│   Usar SOLO cuando la dependencia es CRÍTICA                │
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
│                                                             │
│   Como Requires=, pero MÁS ESTRICTO:                       │
│                                                             │
│   Requires= → si la dependencia se detiene,                 │
│               esta unidad SE DETIENE                        │
│                                                             │
│   BindsTo=  → si la dependencia entra en CUALQUIER          │
│               estado INACTIVO, esta unidad se DETIENE       │
│                                                             │
│   Útil para: dispositivos .device, mounts .mount            │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Unit]
Description=RAID Monitoring
BindsTo=mdraid.service
After=mdraid.service
# Si mdraid.service entra en failed/inactive → nos detenemos
```

### Requisite= — Ya debe estar corriendo

```
┌─────────────────────────────────────────────────────────────┐
│                       Requisite=                            │
│                                                             │
│   Requisite=nginx.service                                   │
│                                                             │
│   1. NO intenta arrancar nginx.service                      │
│   2. Si nginx YA está activo → esta unidad arranca          │
│   3. Si nginx NO está activo → esta unidad FALLA            │
│                                                             │
│   Para verificar que algo ya está corriendo sin arrancarlo  │
└─────────────────────────────────────────────────────────────┘
```

```ini
[Unit]
Description=Configuration Reloader
Requisite=nginx.service
After=nginx.service
# Si nginx no está corriendo → falla inmediatamente
```

### PartOf= — Propagación de stop/restart

```
┌─────────────────────────────────────────────────────────────┐
│                        PartOf=                              │
│                                                             │
│   PartOf=myapp.service                                      │
│                                                             │
│   Si myapp.service se DETIENE o REINICIA →                  │
│   esta unidad también se DETIENE o REINICIA                 │
│                                                             │
│   NO propaga el START                                       │
│   (para re-arrancar workers, el servicio principal          │
│    necesita Wants= hacia los workers)                       │
│                                                             │
│   Ideal para: workers, sidecars, componentes auxiliares     │
└─────────────────────────────────────────────────────────────┘
```

```ini
# Ejemplo: app principal + worker + logger
# myapp.service (principal)
[Unit]
Description=My Application
Wants=myapp-worker.service myapp-logger.service
# Wants= causa que los workers arranquen

# myapp-worker.service
[Unit]
Description=My App Background Worker
PartOf=myapp.service
After=myapp.service
# PartOf= causa que se detenga/reinicie con myapp

# myapp-logger.service
[Unit]
Description=My App Logger
PartOf=myapp.service
After=myapp.service

# systemctl restart myapp:
# 1. myapp se detiene
# 2. myapp-worker se detiene automáticamente (PartOf propaga stop)
# 3. myapp-logger se detiene automáticamente (PartOf propaga stop)
# 4. myapp arranca
# 5. myapp-worker arranca (Wants= en myapp lo causa)
# 6. myapp-logger arranca (Wants= en myapp lo causa)
```

## Resumen de directivas de activación

| Directiva | ¿Activa la dependencia? | Si la dependencia falla | Si la dependencia se detiene |
|---|---|---|---|
| `Wants=` | Sí (intenta) | Arranca igual | Sigue corriendo |
| `Requires=` | Sí | No arranca | Se detiene |
| `BindsTo=` | Sí | No arranca | Se detiene (cualquier estado inactivo) |
| `Requisite=` | No | Falla inmediatamente | Se detiene |
| `PartOf=` | No | — | Se detiene/reinicia con la otra |

## Dependencias de ordenamiento

### After= y Before=

```ini
# After= — arrancar DESPUÉS de la unidad indicada:
[Unit]
After=network.target postgresql.service
# Si network.target y postgresql.service están activos o arrancando,
# esperar a que terminen antes de arrancar esta unidad

# Before= — arrancar ANTES de la unidad indicada:
[Unit]
Before=nginx.service
# Si nginx.service va a arrancar, esperar a que esta unidad arranque primero
```

```
┌─────────────────────────────────────────────────────────────┐
│  After=/Before= NO ACTIVAN nada — solo definen ORDEN        │
│                                                             │
│   After=network.target                                      │
│   → SI network.target está activo: esperamos                │
│   → SI network.target NO está activo: After se IGNORA       │
│                                                             │
│   Por eso se combina con Wants=/Requires:                   │
│   Wants=network.target   → intentamos arrancar              │
│   After=network.target   → esperamos a que esté activo      │
└─────────────────────────────────────────────────────────────┘
```

### Sin ordenamiento — Arranque en paralelo

```ini
# Sin After=/Before=, las unidades arrancan EN PARALELO:
[Unit]
Wants=redis.service
Wants=memcached.service
Wants=elasticsearch.service

# Las tres arrancan SIMULTÁNEAMENTE
# → Boot más rápido (paralelismo)
# → Bien para servicios independientes entre sí
```

## Conflicts= — Incompatibilidad

```ini
[Unit]
Conflicts=apache2.service
# → Al arrancar esta unidad, apache2 se detiene automáticamente
# → Al arrancar apache2, esta unidad se detiene
# → Garantiza que ambas no corran simultáneamente

# Caso real: firewalld vs iptables
# firewalld.service:
[Unit]
Conflicts=iptables.service ip6tables.service ebtables.service
```

## Targets como puntos de sincronización

Los targets agrupan dependencias y sirven como puntos de sincronización en el boot:

```
boot timeline:

systemd (PID 1)
    │
    ▼
local-fs.target ─── swap.target
    │
    ▼
sysinit.target         ← inicialización básica (udev, journal, mounts)
    │
    ▼
basic.target           ← servicios básicos (D-Bus, sockets, timers)
    │
    ├──► sockets.target    ← todos los sockets escuchando
    ├──► timers.target     ← todos los timers activos
    │
    ▼
network.target         ← interfaces con IP asignada
    │
    ▼
network-online.target  ← conectividad REAL verificada
    │
    ▼
multi-user.target      ← modo multiusuario (sin GUI)
    │
    ▼
graphical.target       ← modo gráfico (con GUI)
```

### network.target vs network-online.target

```
network.target:
  → Las interfaces tienen IP asignada
  → bind() a un puerto funciona
  → PERO puede no haber ruta a Internet
  → Suficiente para: servidores que ESCUCHAN (nginx, sshd)

network-online.target:
  → Hay conectividad verificada (ruta, DNS)
  → Suficiente para: servicios que CONECTAN a hosts remotos
  → Más lento — espera propagación de rutas
  → Requiere Wants= porque no se activa por defecto
```

```ini
# Servidor que escucha (no necesita red completa):
[Unit]
Description=HTTP Server
After=network.target
# IP asignada = suficiente para bind()

# Cliente que conecta a base de datos remota:
[Unit]
Description=My App
After=network-online.target
Wants=network-online.target
# Necesita red REAL para conectar a la DB
# Wants= es necesario porque network-online.target no se activa solo
```

### Targets importantes

```bash
sysinit.target         # udev, tmpfiles, journal, swap
basic.target           # D-Bus, sockets base, timers base
sockets.target         # todos los sockets (paralelismo en boot)
timers.target          # todos los timers
local-fs.target        # sistemas de archivos locales montados
remote-fs.target       # sistemas de archivos remotos (NFS)
network.target         # interfaces con IP
network-online.target  # conectividad real verificada
multi-user.target      # modo multiusuario (sin GUI)
graphical.target       # modo gráfico (GDM, SDDM)
```

## Visualizar dependencias

```bash
# Árbol de dependencias (qué NECESITA esta unidad):
systemctl list-dependencies nginx.service

# Dependencias inversas (qué DEPENDE de esta unidad):
systemctl list-dependencies nginx.service --reverse

# Todas las dependencias (recursivo):
systemctl list-dependencies nginx.service --all

# Target por defecto y sus dependencias:
systemctl list-dependencies "$(systemctl get-default)"
```

```bash
# systemd-analyze: tiempos y cadenas críticas
systemd-analyze                      # tiempo total del boot
systemd-analyze blame                # servicios más lentos
systemd-analyze critical-chain       # cadena crítica

# Ejemplo de critical-chain:
# multi-user.target @15.234s
# └─nginx.service @10.500s +4.734s
#   └─network-online.target @10.050s +0.450s
#     └─NetworkManager.service @3.000s +7.050s

# Generar imagen SVG del boot:
systemd-analyze plot > /tmp/boot.svg
```

## DefaultDependencies — Dependencias implícitas

Por defecto, systemd agrega dependencias automáticamente:

```ini
[Unit]
DefaultDependencies=yes   # es el default (implícito)

# systemd agrega IMPLÍCITAMENTE:
# After=sysinit.target basic.target
# Conflicts=shutdown.target
# Before=shutdown.target

# Esto asegura que todo servicio normal:
# → Arranque después de la inicialización del sistema
# → Se detenga al apagar
```

```ini
# Para unidades que necesitan arrancar MUY temprano (antes de sysinit):
[Unit]
DefaultDependencies=no
# CUIDADO: necesitas definir todas las dependencias manualmente
```

## Errores comunes

### Requires= sin After=

```ini
# ERROR SUTIL: Requires sin After = arranque en paralelo
[Unit]
Requires=postgresql.service
# postgresql y esta unidad ARRANCAN SIMULTÁNEAMENTE
# Si la unidad necesita que postgresql esté listo → puede fallar

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

# CORRECTO:
[Unit]
After=network-online.target
Wants=network-online.target
# Wants= necesario porque network-online.target no se activa por defecto
```

### Loop de dependencias

```bash
# Si A After B, y B After A → DEPENDENCY CYCLE
# systemd[1]: Found ordering cycle on myapp.service
# systemd intentará romper el ciclo y seguirá

# Detectar ciclos:
systemd-analyze verify myapp.service

# Solución: revisar la cadena de After/Before y romper el ciclo
```

### Wants= y WantedBy= son cosas distintas

```ini
[Unit]
Wants=redis.service         # ← dependencia de activación (en [Unit])

[Install]
WantedBy=multi-user.target  # ← cómo se habilita al boot (en [Install])
# enable crea symlink en multi-user.target.wants/
```

---

## Lab — Dependencias

### Objetivo

Dominar las dependencias en systemd: los dos ejes independientes
(activación con Wants/Requires y ordenamiento con After/Before),
PartOf para propagación de stop/restart, Conflicts para
incompatibilidad, targets como puntos de sincronización, y
detección de errores con systemd-analyze verify.

### Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

### Parte 1 — Activación y ordenamiento

#### Paso 1.1: Los dos ejes

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos ejes independientes ==="
echo ""
echo "1. ACTIVACIÓN — ¿se arranca la otra unidad?"
echo "   Wants=, Requires=, BindsTo=, Requisite="
echo ""
echo "2. ORDENAMIENTO — ¿en qué orden arrancan?"
echo "   After=, Before="
echo ""
echo "Son INDEPENDIENTES:"
echo "  Requires= SIN After= → arrancan en PARALELO"
echo "  After= SIN Requires= → define orden pero no arranca"
echo ""
echo "--- Patrón habitual: activación + orden ---"
echo "[Unit]"
echo "Requires=postgresql.service    # arrancar postgresql"
echo "After=postgresql.service       # esperar a que esté listo"
echo ""
echo "--- Solo orden ---"
echo "[Unit]"
echo "After=postgresql.service       # si está activo, arrancar después"
echo ""
echo "--- Solo activación (paralelo) ---"
echo "[Unit]"
echo "Wants=redis.service            # arrancar redis, sin esperar"
'
```

#### Paso 1.2: Wants vs Requires

Antes de ejecutar, predice: si un servicio usa `Wants=redis.service`
y redis falla al arrancar, ¿qué pasa con el servicio?

```bash
docker compose exec debian-dev bash -c '
echo "=== Wants vs Requires ==="
echo ""
echo "--- Wants= (dependencia débil) ---"
cat > /tmp/lab-wants.service << '\''EOF'\''
[Unit]
Description=Service with Wants
Wants=redis.service
After=redis.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-wants.service
echo ""
echo "Wants=redis.service:"
echo "  Intenta arrancar redis"
echo "  Si redis FALLA, este servicio arranca IGUAL"
echo "  Si redis SE DETIENE después, este sigue corriendo"
echo "  Es la dependencia más usada — tolerante a fallos"
echo ""
echo "--- Requires= (dependencia fuerte) ---"
cat > /tmp/lab-requires.service << '\''EOF'\''
[Unit]
Description=Service with Requires
Requires=postgresql.service
After=postgresql.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=multi-user.target
EOF
cat -n /tmp/lab-requires.service
echo ""
echo "Requires=postgresql.service:"
echo "  Arranca postgresql"
echo "  Si postgresql FALLA, este servicio NO arranca"
echo "  Si postgresql SE DETIENE, este se detiene TAMBIÉN"
echo "  Usar cuando el servicio NO puede funcionar sin la dependencia"
'
```

#### Paso 1.3: BindsTo, Requisite y PartOf

```bash
docker compose exec debian-dev bash -c '
echo "=== BindsTo, Requisite, PartOf ==="
echo ""
echo "--- BindsTo= (muy fuerte) ---"
echo "Como Requires, pero aún más estricto:"
echo "  Si la dependencia entra en CUALQUIER estado inactivo,"
echo "  esta unidad se detiene"
echo "  Útil para unidades que dependen de dispositivos o mounts"
echo ""
echo "--- Requisite= (ya activa) ---"
echo "  NO arranca la dependencia"
echo "  Si ya está activa → esta unidad arranca"
echo "  Si NO está activa → falla inmediatamente"
echo "  Verifica que algo ya está corriendo sin arrancarlo"
echo ""
echo "--- PartOf= (propagación de stop/restart) ---"
cat > /tmp/lab-partof.service << '\''EOF'\''
[Unit]
Description=Worker (PartOf main app)
PartOf=myapp.service
After=myapp.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF
cat -n /tmp/lab-partof.service
echo ""
echo "PartOf=myapp.service:"
echo "  Si myapp se DETIENE o REINICIA, este worker también"
echo "  NO propaga el start (no arranca al arrancar myapp)"
echo "  Para que arranque, myapp necesita Wants= hacia el worker"
echo "  Útil para componentes auxiliares (workers, sidecars)"
echo ""
echo "--- Resumen ---"
echo "| Directiva  | Arranca dep? | Si dep falla | Si dep se detiene |"
echo "|------------|-------------|-------------|-------------------|"
echo "| Wants      | Sí          | Sigue       | Sigue             |"
echo "| Requires   | Sí          | No arranca  | Se detiene        |"
echo "| BindsTo    | Sí          | No arranca  | Se detiene (cualq)|"
echo "| Requisite  | No          | Falla       | Se detiene        |"
echo "| PartOf     | No          | —           | Se detiene        |"
'
```

#### Paso 1.4: After/Before y Conflicts

```bash
docker compose exec debian-dev bash -c '
echo "=== After/Before y Conflicts ==="
echo ""
echo "--- After= / Before= ---"
echo "Definen ORDEN, no activan la dependencia"
echo ""
echo "After=network.target:"
echo "  Si network.target está activo o arrancando,"
echo "  esperar a que termine antes de arrancar esta unidad"
echo ""
echo "Before=nginx.service:"
echo "  Si nginx va a arrancar, esperar a que esta unidad"
echo "  arranque primero"
echo ""
echo "Sin After/Before → arranque en PARALELO"
echo "(bueno para rendimiento del boot)"
echo ""
echo "--- Conflicts= ---"
cat > /tmp/lab-conflicts.service << '\''EOF'\''
[Unit]
Description=Service with Conflicts
Conflicts=apache2.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF
echo ""
echo "Conflicts=apache2.service:"
echo "  Si apache2 está corriendo, se DETIENE al arrancar esta unidad"
echo "  Si esta unidad está corriendo, se DETIENE al arrancar apache2"
echo "  Garantiza que ambas no corran simultáneamente"
echo ""
echo "Caso típico: iptables vs firewalld"
'
```

---

### Parte 2 — Targets y sincronización

#### Paso 2.1: Cadena de boot

```bash
docker compose exec debian-dev bash -c '
echo "=== Cadena de boot ==="
echo ""
echo "local-fs.target ─── swap.target"
echo "        │"
echo "   sysinit.target"
echo "        │"
echo "   basic.target"
echo "        │"
echo "  ┌─────┼──────────────┐"
echo "  │     │              │"
echo "sockets.target  timers.target  paths.target"
echo "  │     │              │"
echo "  └─────┼──────────────┘"
echo "        │"
echo "network.target ──── network-online.target"
echo "        │"
echo "multi-user.target"
echo "        │"
echo "graphical.target  (si es el default)"
echo ""
echo "Los targets son puntos de sincronización:"
echo "  sysinit.target  = udev, tmpfiles, journal, swap"
echo "  basic.target    = D-Bus, sockets base, timers base"
echo "  network.target  = interfaces con IP asignada"
echo "  multi-user.target = servidor completo sin GUI"
'
```

#### Paso 2.2: network.target vs network-online.target

```bash
docker compose exec debian-dev bash -c '
echo "=== network.target vs network-online.target ==="
echo ""
echo "--- network.target ---"
echo "Las interfaces tienen IP asignada"
echo "PERO puede no haber conectividad real (DHCP en progreso)"
echo "Suficiente para servicios que ESCUCHAN (bind a IP/puerto)"
echo ""
echo "  After=network.target"
echo ""
echo "--- network-online.target ---"
echo "Conectividad real verificada (ruta, DNS)"
echo "Necesario para servicios que CONECTAN a hosts remotos al arrancar"
echo ""
echo "  After=network-online.target"
echo "  Wants=network-online.target"
echo ""
echo "IMPORTANTE: Wants= es necesario porque network-online.target"
echo "no se activa por defecto"
echo ""
echo "--- Verificar en archivos reales ---"
echo "Servicios que usan network.target:"
grep -rl "After=network.target" /usr/lib/systemd/system/*.service 2>/dev/null | \
    xargs -I{} basename {} | head -5
echo ""
echo "Servicios que usan network-online.target:"
grep -rl "network-online.target" /usr/lib/systemd/system/*.service 2>/dev/null | \
    xargs -I{} basename {} | head -5
'
```

#### Paso 2.3: Dependencias en unit files reales

```bash
docker compose exec debian-dev bash -c '
echo "=== Dependencias en unit files reales ==="
echo ""
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    AFTER=$(grep "^After=" "$svc" 2>/dev/null)
    WANTS=$(grep "^Wants=" "$svc" 2>/dev/null)
    REQUIRES=$(grep "^Requires=" "$svc" 2>/dev/null)
    if [[ -n "$AFTER" || -n "$WANTS" || -n "$REQUIRES" ]]; then
        echo "--- $(basename "$svc") ---"
        [[ -n "$AFTER" ]] && echo "  $AFTER"
        [[ -n "$WANTS" ]] && echo "  $WANTS"
        [[ -n "$REQUIRES" ]] && echo "  $REQUIRES"
        echo ""
    fi
done | head -40
echo "..."
'
```

---

### Parte 3 — Verificación y errores

#### Paso 3.1: Requires sin After

Antes de ejecutar, predice: ¿qué pasa si usas `Requires=` sin `After=`?

```bash
docker compose exec debian-dev bash -c '
echo "=== Requires sin After ==="
echo ""
echo "--- INCORRECTO ---"
cat > /tmp/lab-noafter.service << '\''EOF'\''
[Unit]
Description=Service without After
Requires=postgresql.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF
echo "[Unit]"
echo "Requires=postgresql.service"
echo "# SIN After= → arrancan en PARALELO"
echo "# Si necesitas que postgresql esté listo → puede fallar"
echo ""
echo "--- CORRECTO ---"
cat > /tmp/lab-withafter.service << '\''EOF'\''
[Unit]
Description=Service with After
Requires=postgresql.service
After=postgresql.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF
echo "[Unit]"
echo "Requires=postgresql.service"
echo "After=postgresql.service"
echo "# Arranca postgresql Y espera a que esté listo"
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-noafter.service 2>&1 || true
echo ""
systemd-analyze verify /tmp/lab-withafter.service 2>&1 || true
'
```

#### Paso 3.2: Dependency loops

```bash
docker compose exec debian-dev bash -c '
echo "=== Dependency loops ==="
echo ""
echo "Si A After B, y B After A → loop de dependencias"
echo ""
cat > /tmp/lab-loop-a.service << '\''EOF'\''
[Unit]
Description=Loop A
After=lab-loop-b.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF

cat > /tmp/lab-loop-b.service << '\''EOF'\''
[Unit]
Description=Loop B
After=lab-loop-a.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF
echo "lab-loop-a.service: After=lab-loop-b.service"
echo "lab-loop-b.service: After=lab-loop-a.service"
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-loop-a.service 2>&1 || true
echo ""
echo "systemd detecta loops y los reporta"
echo "Solución: revisar la cadena de After/Before y romper el ciclo"
rm /tmp/lab-loop-a.service /tmp/lab-loop-b.service
'
```

#### Paso 3.3: DefaultDependencies

```bash
docker compose exec debian-dev bash -c '
echo "=== DefaultDependencies ==="
echo ""
echo "La mayoría de unidades tiene DefaultDependencies=yes (implícito)"
echo "Esto agrega automáticamente:"
echo "  After=sysinit.target basic.target"
echo "  Conflicts=shutdown.target"
echo "  Before=shutdown.target"
echo ""
echo "Esto asegura que:"
echo "  - Los servicios arrancan DESPUÉS de la inicialización"
echo "  - Se detienen al apagar"
echo ""
echo "Para unidades de early boot:"
echo "  [Unit]"
echo "  DefaultDependencies=no"
echo ""
echo "--- Buscar unidades con DefaultDependencies=no ---"
grep -rl "DefaultDependencies=no" /usr/lib/systemd/system/*.service 2>/dev/null | \
    xargs -I{} basename {} | head -5
echo "(estas son unidades que arrancan MUY temprano)"
'
```

#### Paso 3.4: Ejemplo completo con dependencias

```bash
docker compose exec debian-dev bash -c '
echo "=== Ejemplo: app con dependencias ==="
echo ""
echo "--- myapp.target (agrupa el stack) ---"
cat > /tmp/lab-myapp.target << '\''EOF'\''
[Unit]
Description=My Application Stack
Requires=multi-user.target
After=multi-user.target
EOF
cat -n /tmp/lab-myapp.target
echo ""
echo "--- myapp-api.service ---"
cat > /tmp/lab-myapp-api.service << '\''EOF'\''
[Unit]
Description=API Server
After=network-online.target
Wants=network-online.target
PartOf=lab-myapp.target

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=lab-myapp.target
EOF
cat -n /tmp/lab-myapp-api.service
echo ""
echo "--- myapp-worker.service ---"
cat > /tmp/lab-myapp-worker.service << '\''EOF'\''
[Unit]
Description=Background Worker
After=lab-myapp-api.service
PartOf=lab-myapp.target

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=lab-myapp.target
EOF
cat -n /tmp/lab-myapp-worker.service
echo ""
echo "PartOf= hace que al reiniciar myapp.target,"
echo "tanto api como worker se reinicien"
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-myapp-api.service 2>&1 || true
rm /tmp/lab-myapp.target /tmp/lab-myapp-api.service /tmp/lab-myapp-worker.service
'
```

---

### Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/lab-wants.service /tmp/lab-requires.service
rm -f /tmp/lab-partof.service /tmp/lab-conflicts.service
rm -f /tmp/lab-noafter.service /tmp/lab-withafter.service
echo "Archivos temporales eliminados"
'
```

### Conceptos reforzados

1. Las dependencias tienen dos ejes **independientes**: activación
   (`Wants=`, `Requires=`) y ordenamiento (`After=`, `Before=`).
   `Requires=` sin `After=` arranca en paralelo.

2. `Wants=` es tolerante a fallos (la dependencia puede fallar).
   `Requires=` es estricto (falla si la dependencia falla).
   `BindsTo=` es aún más estricto (cualquier estado inactivo).

3. `PartOf=` propaga stop/restart pero NO start. Para que los
   workers re-arranquen, el servicio principal necesita `Wants=`
   hacia ellos.

4. `network.target` = interfaces con IP (para servicios que
   escuchan). `network-online.target` = conectividad real (para
   servicios que conectan). El segundo requiere `Wants=`.

5. `DefaultDependencies=yes` (implícito) agrega automáticamente
   dependencias de sysinit y shutdown. Solo desactivar para
   unidades de early boot.

6. `systemd-analyze verify` detecta dependency loops y errores
   de configuración en archivos de unidad.

---

## Ejercicios

### Ejercicio 1 — Explorar dependencias de un servicio

Usa `systemctl list-dependencies` para ver de qué depende `sshd.service`. Luego usa `--reverse` para ver qué depende de `network.target`. ¿Cuántas unidades dependen del target por defecto?

```bash
# Dependencias de sshd:
systemctl list-dependencies sshd.service

# Dependencias inversas de network.target:
systemctl list-dependencies network.target --reverse | head -20

# Unidades del target por defecto:
systemctl list-dependencies "$(systemctl get-default)" --no-pager | wc -l
```

<details><summary>Predicción</summary>

- `sshd.service` depende de targets fundamentales: `sysinit.target`, `basic.target`, probablemente `network.target`. Estas vienen de `DefaultDependencies=yes` y de los `After=` explícitos en su unit file.
- `network.target --reverse` muestra todos los servicios con `After=network.target` o `Wants=network.target`: servicios de red como sshd, cron, etc.
- El target por defecto (`multi-user.target` o `graphical.target`) tiene muchas dependencias: todos los servicios enabled más los targets que necesita. Típicamente 50-100+ unidades.
- `list-dependencies` sin `--all` muestra solo dependencias directas; con `--all` muestra el árbol recursivo completo.

</details>

### Ejercicio 2 — Propiedades de dependencia con show

Usa `systemctl show` para ver las propiedades `After`, `Requires`, `Wants`, y `Conflicts` de un servicio. Compara con lo que muestra `list-dependencies`.

```bash
# Propiedades de dependencia:
systemctl show sshd --property=After,Requires,Wants,Conflicts --no-pager

# Comparar con list-dependencies:
systemctl list-dependencies sshd.service --no-pager
```

<details><summary>Predicción</summary>

`systemctl show --property=After` muestra **todas** las dependencias After, incluyendo las implícitas de `DefaultDependencies=yes` (como `sysinit.target`, `basic.target`). Hay más de las que aparecen en el unit file.

`list-dependencies` muestra el árbol visual de dependencias resueltas. Las dependencias implícitas aparecen aquí también.

La diferencia: `show` devuelve las propiedades como texto plano (parseable), `list-dependencies` las presenta como árbol visual (para humanos). `show` incluye absolutamente todo, mientras `list-dependencies` sin `--all` muestra solo el primer nivel.

</details>

### Ejercicio 3 — Tiempos de boot

Analiza el boot del sistema usando `systemd-analyze`. Identifica los 5 servicios más lentos y la cadena crítica.

```bash
# Tiempo total del boot:
systemd-analyze

# Los 5 más lentos:
systemd-analyze blame --no-pager | head -5

# Cadena crítica:
systemd-analyze critical-chain --no-pager 2>&1 | head -15
```

<details><summary>Predicción</summary>

- `systemd-analyze` muestra tres tiempos: kernel (tiempo en el kernel antes de systemd), initrd (si usa initramfs), y userspace (desde que systemd arranca hasta que el target por defecto está activo).
- `blame` lista servicios ordenados por tiempo de arranque. Los más lentos suelen ser: `cloud-init`, `NetworkManager-wait-online`, `plymouth`, `docker`, o `firewalld`.
- `critical-chain` muestra la cadena de dependencias que determinó el tiempo total del boot. Solo aparecen las unidades que fueron el cuello de botella. El formato muestra `@timestamp +duration` para cada unidad: `@` es cuándo arrancó, `+` es cuánto tardó.

</details>

### Ejercicio 4 — Wants vs Requires en la práctica

Crea dos servicios: uno con `Wants=` y otro con `Requires=` hacia un servicio inexistente. Intenta arrancar ambos. ¿Cuál arranca y cuál falla?

```bash
# Servicio con Wants (dependencia débil):
sudo tee /etc/systemd/system/test-wants.service << 'EOF'
[Unit]
Description=Test Wants
Wants=inexistente.service
After=inexistente.service

[Service]
Type=oneshot
ExecStart=/bin/echo "Wants: arrancó OK"
RemainAfterExit=yes
EOF

# Servicio con Requires (dependencia fuerte):
sudo tee /etc/systemd/system/test-requires.service << 'EOF'
[Unit]
Description=Test Requires
Requires=inexistente.service
After=inexistente.service

[Service]
Type=oneshot
ExecStart=/bin/echo "Requires: arrancó OK"
RemainAfterExit=yes
EOF

sudo systemctl daemon-reload
sudo systemctl start test-wants 2>&1; echo "Exit: $?"
sudo systemctl start test-requires 2>&1; echo "Exit: $?"
```

<details><summary>Predicción</summary>

- `test-wants` **ARRANCA correctamente**. `Wants=` intenta arrancar `inexistente.service`, falla silenciosamente, pero el servicio principal arranca igual. El echo muestra "Wants: arrancó OK".
- `test-requires` **FALLA**. `Requires=` intenta arrancar `inexistente.service`, que no existe, y como la dependencia falla, el servicio principal no arranca. El exit code es no-cero.

Esta es la diferencia fundamental: `Wants=` tolera fallos, `Requires=` no.

Limpiar:
```bash
sudo systemctl stop test-wants test-requires 2>/dev/null
sudo rm /etc/systemd/system/test-wants.service /etc/systemd/system/test-requires.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 5 — PartOf en acción

Crea un servicio principal y un worker con `PartOf`. Verifica que al reiniciar el principal, el worker se reinicia. ¿Qué pasa si arrancas solo el worker sin el principal?

```bash
# Principal:
sudo tee /etc/systemd/system/main-app.service << 'EOF'
[Unit]
Description=Main Application
Wants=main-worker.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=multi-user.target
EOF

# Worker:
sudo tee /etc/systemd/system/main-worker.service << 'EOF'
[Unit]
Description=Worker
PartOf=main-app.service
After=main-app.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF

sudo systemctl daemon-reload
sudo systemctl start main-app
systemctl is-active main-app main-worker

# Reiniciar el principal:
sudo systemctl restart main-app
systemctl is-active main-app main-worker
```

<details><summary>Predicción</summary>

- Al hacer `start main-app`: el principal arranca, y `Wants=main-worker.service` causa que el worker también arranque. Ambos quedan `active`.
- Al hacer `restart main-app`:
  1. `main-app` se detiene → `PartOf=` propaga: `main-worker` se detiene
  2. `main-app` arranca → `Wants=` causa: `main-worker` arranca
  3. Ambos quedan `active`
- Si arrancas solo el worker sin el principal: funciona. `PartOf=` no impide arrancar el worker independientemente; solo propaga stop/restart.
- Si detienes el principal: el worker se detiene automáticamente (PartOf).

Limpiar:
```bash
sudo systemctl stop main-app main-worker
sudo rm /etc/systemd/system/main-app.service /etc/systemd/system/main-worker.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 6 — Conflicts

Crea dos servicios con `Conflicts=` entre ellos. Arranca uno, verifica que está activo, luego arranca el otro. ¿Qué pasa con el primero?

```bash
sudo tee /etc/systemd/system/svc-alpha.service << 'EOF'
[Unit]
Description=Service Alpha
Conflicts=svc-beta.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF

sudo tee /etc/systemd/system/svc-beta.service << 'EOF'
[Unit]
Description=Service Beta
Conflicts=svc-alpha.service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
EOF

sudo systemctl daemon-reload
sudo systemctl start svc-alpha
systemctl is-active svc-alpha svc-beta

# Arrancar beta:
sudo systemctl start svc-beta
systemctl is-active svc-alpha svc-beta
```

<details><summary>Predicción</summary>

- Después de `start svc-alpha`: alpha está `active`, beta está `inactive`
- Después de `start svc-beta`: beta arranca y alpha se **detiene automáticamente** por `Conflicts=`
- Ahora alpha está `inactive` y beta está `active`
- `Conflicts=` garantiza exclusión mutua: solo uno puede correr a la vez
- Caso real: `firewalld` vs `iptables` — al instalar uno, el otro se detiene

Limpiar:
```bash
sudo systemctl stop svc-alpha svc-beta 2>/dev/null
sudo rm /etc/systemd/system/svc-alpha.service /etc/systemd/system/svc-beta.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 7 — network.target vs network-online.target

Busca en los unit files del sistema cuáles usan `network.target` y cuáles `network-online.target`. ¿Por qué un servicio elegiría uno u otro?

```bash
echo "=== Servicios con After=network.target ==="
grep -rl "After=network.target" /usr/lib/systemd/system/*.service 2>/dev/null | \
    xargs -I{} basename {} | head -10

echo ""
echo "=== Servicios con network-online.target ==="
grep -rl "network-online.target" /usr/lib/systemd/system/*.service 2>/dev/null | \
    xargs -I{} basename {} | head -10
```

<details><summary>Predicción</summary>

- `network.target`: lo usan servicios que **escuchan** en un puerto (sshd, nginx, httpd). Solo necesitan que las interfaces tengan IP para hacer `bind()`. No necesitan conectividad completa.
- `network-online.target`: lo usan servicios que **conectan** a hosts remotos al arrancar (clientes NFS, servicios que descargan configuración remota, monitoreo que envía datos). Necesitan que haya rutas y DNS funcional.

La razón de la distinción: `network-online.target` es más lento porque espera verificación de conectividad (DHCP completo, route check). Si todos los servicios esperaran a `network-online.target`, el boot sería innecesariamente lento.

`network-online.target` requiere `Wants=` explícito porque no se activa por defecto — un servicio que lo necesita debe declarar tanto `After=` como `Wants=`.

</details>

### Ejercicio 8 — Dependency loop

Crea intencionalmente un loop de dependencias (A After B, B After A). Usa `systemd-analyze verify` para detectarlo. ¿Qué hace systemd cuando encuentra un ciclo?

```bash
sudo tee /etc/systemd/system/loop-a.service << 'EOF'
[Unit]
Description=Loop A
After=loop-b.service
Requires=loop-b.service

[Service]
Type=oneshot
ExecStart=/bin/echo "A"
RemainAfterExit=yes
EOF

sudo tee /etc/systemd/system/loop-b.service << 'EOF'
[Unit]
Description=Loop B
After=loop-a.service
Requires=loop-a.service

[Service]
Type=oneshot
ExecStart=/bin/echo "B"
RemainAfterExit=yes
EOF

sudo systemctl daemon-reload
systemd-analyze verify loop-a.service 2>&1
```

<details><summary>Predicción</summary>

`systemd-analyze verify` reporta el ciclo:
```
Found ordering cycle on loop-a.service/start
```

Cuando systemd encuentra un ciclo en producción:
1. Registra un warning en el journal: "Found ordering cycle"
2. **Intenta romper el ciclo** eliminando una de las relaciones de orden
3. Arranca los servicios (posiblemente en paralelo) — no falla completamente
4. El resultado es impredecible: uno puede arrancar antes que el otro

La solución es revisar la cadena de After/Before y eliminar una de las dependencias circulares. En la práctica, A probablemente debería ser After B (pero no al revés), o usar una arquitectura diferente.

Limpiar:
```bash
sudo systemctl stop loop-a loop-b 2>/dev/null
sudo rm /etc/systemd/system/loop-a.service /etc/systemd/system/loop-b.service
sudo systemctl daemon-reload
```

</details>

### Ejercicio 9 — DefaultDependencies

Busca unidades del sistema que usen `DefaultDependencies=no`. ¿Por qué lo necesitan? Compara las dependencias implícitas de un servicio normal vs uno con `DefaultDependencies=no`.

```bash
# Unidades con DefaultDependencies=no:
grep -rl "DefaultDependencies=no" /usr/lib/systemd/system/*.service 2>/dev/null | \
    xargs -I{} basename {} | head -10

# Comparar dependencias implícitas:
echo "=== Servicio normal (DefaultDependencies=yes) ==="
systemctl show sshd --property=After --no-pager 2>/dev/null | head -3

echo ""
echo "=== Servicio con DefaultDependencies=no ==="
# Elegir uno de los encontrados arriba
```

<details><summary>Predicción</summary>

Los servicios con `DefaultDependencies=no` son unidades de **early boot** que deben arrancar antes de `sysinit.target`:
- `systemd-journald.service` — logging debe estar disponible desde el inicio
- `systemd-udevd.service` — detección de dispositivos
- `systemd-tmpfiles-setup.service` — creación de archivos temporales

Sin `DefaultDependencies=no`, estos servicios tendrían `After=sysinit.target basic.target`, lo que crearía un **ciclo**: sysinit.target depende de ellos, y ellos dependerían de sysinit.target.

Un servicio normal con `DefaultDependencies=yes` tiene muchas dependencias After implícitas (sysinit.target, basic.target). Uno con `DefaultDependencies=no` solo tiene las dependencias explícitamente declaradas en su unit file.

</details>

### Ejercicio 10 — Diseñar dependencias para un stack

Diseña las dependencias para un stack de aplicación con: API server, worker de background, y health checker. Requisitos:
- API necesita red y PostgreSQL
- Worker necesita que API esté corriendo
- Health checker necesita que API esté corriendo (pero no lo arranca)
- Al reiniciar API, worker y health checker se reinician

```bash
# ¿Qué directivas usarías para cada servicio?
echo "=== API Server ==="
echo "[Unit]"
echo "After=network-online.target postgresql.service"
echo "Wants=network-online.target"
echo "Requires=postgresql.service"
echo "Wants=api-worker.service api-health.service"
echo ""

echo "=== Worker ==="
echo "[Unit]"
echo "After=api-server.service"
echo "PartOf=api-server.service"
echo ""

echo "=== Health Checker ==="
echo "[Unit]"
echo "After=api-server.service"
echo "Requisite=api-server.service"
echo "PartOf=api-server.service"
```

<details><summary>Predicción</summary>

**API Server** (`api-server.service`):
- `After=network-online.target` + `Wants=network-online.target` — necesita red real para conectar a PostgreSQL remoto
- `Requires=postgresql.service` + `After=postgresql.service` — dependencia fuerte, no puede funcionar sin DB
- `Wants=api-worker.service api-health.service` — arranca los componentes auxiliares

**Worker** (`api-worker.service`):
- `After=api-server.service` — arranca después del API
- `PartOf=api-server.service` — se reinicia cuando API se reinicia

**Health Checker** (`api-health.service`):
- `After=api-server.service` — arranca después del API
- `Requisite=api-server.service` — falla si API no está ya activo (no lo arranca por sí solo)
- `PartOf=api-server.service` — se reinicia cuando API se reinicia

¿Por qué `Requisite=` para el health checker? Porque no tiene sentido correr un health check si API no está activo. Y no queremos que el health checker sea quien arranque el API — eso lo hace el sistema vía `enable`.

¿Por qué no `Requires=` para el worker? Porque `PartOf=` ya maneja la propagación de stop. Y `Wants=` en el API ya causa que arranque. Agregar `Requires=` en el worker hacia el API sería redundante.

</details>
