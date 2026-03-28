# Lab — Dependencias

## Objetivo

Dominar las dependencias en systemd: los dos ejes independientes
(activacion con Wants/Requires y ordenamiento con After/Before),
PartOf para propagacion de stop/restart, Conflicts para
incompatibilidad, targets como puntos de sincronizacion, y
deteccion de errores con systemd-analyze verify.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Activacion y ordenamiento

### Objetivo

Entender que activacion y ordenamiento son ejes independientes,
y las diferencias entre Wants, Requires, BindsTo, y Requisite.

### Paso 1.1: Los dos ejes

```bash
docker compose exec debian-dev bash -c '
echo "=== Dos ejes independientes ==="
echo ""
echo "1. ACTIVACION — se arranca la otra unidad?"
echo "   Wants=, Requires=, BindsTo=, Requisite="
echo ""
echo "2. ORDENAMIENTO — en que orden arrancan?"
echo "   After=, Before="
echo ""
echo "Son INDEPENDIENTES:"
echo "  Requires= SIN After= → arrancan en PARALELO"
echo "  After= SIN Requires= → define orden pero no arranca"
echo ""
echo "--- Patron habitual: activacion + orden ---"
echo "[Unit]"
echo "Requires=postgresql.service    # arrancar postgresql"
echo "After=postgresql.service       # esperar a que este listo"
echo ""
echo "--- Solo orden ---"
echo "[Unit]"
echo "After=postgresql.service       # si esta activo, arrancar despues"
echo ""
echo "--- Solo activacion (paralelo) ---"
echo "[Unit]"
echo "Wants=redis.service            # arrancar redis, sin esperar"
'
```

### Paso 1.2: Wants vs Requires

Antes de ejecutar, predice: si un servicio usa `Wants=redis.service`
y redis falla al arrancar, que pasa con el servicio?

```bash
docker compose exec debian-dev bash -c '
echo "=== Wants vs Requires ==="
echo ""
echo "--- Wants= (dependencia debil) ---"
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
echo "  Si redis SE DETIENE despues, este sigue corriendo"
echo "  Es la dependencia mas usada — tolerante a fallos"
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
echo "  Si postgresql SE DETIENE, este se detiene TAMBIEN"
echo "  Usar cuando el servicio NO puede funcionar sin la dependencia"
'
```

### Paso 1.3: BindsTo, Requisite y PartOf

```bash
docker compose exec debian-dev bash -c '
echo "=== BindsTo, Requisite, PartOf ==="
echo ""
echo "--- BindsTo= (muy fuerte) ---"
echo "Como Requires, pero aun mas estricto:"
echo "  Si la dependencia entra en CUALQUIER estado inactivo,"
echo "  esta unidad se detiene"
echo "  Util para unidades que dependen de dispositivos o mounts"
echo ""
echo "--- Requisite= (ya activa) ---"
echo "  NO arranca la dependencia"
echo "  Si ya esta activa → esta unidad arranca"
echo "  Si NO esta activa → falla inmediatamente"
echo "  Verifica que algo ya esta corriendo sin arrancarlo"
echo ""
echo "--- PartOf= (propagacion de stop/restart) ---"
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
echo "  Si myapp se DETIENE o REINICIA, este worker tambien"
echo "  NO propaga el start (no arranca al arrancar myapp)"
echo "  Util para componentes auxiliares (workers, sidecars)"
echo ""
echo "--- Resumen ---"
echo "| Directiva  | Arranca dep? | Si dep falla | Si dep se detiene |"
echo "|------------|-------------|-------------|-------------------|"
echo "| Wants      | Si          | Sigue       | Sigue             |"
echo "| Requires   | Si          | No arranca  | Se detiene        |"
echo "| BindsTo    | Si          | No arranca  | Se detiene (cualquier)|"
echo "| Requisite  | No          | Falla       | —                 |"
echo "| PartOf     | No          | —           | Se detiene        |"
'
```

### Paso 1.4: After/Before y Conflicts

```bash
docker compose exec debian-dev bash -c '
echo "=== After/Before y Conflicts ==="
echo ""
echo "--- After= / Before= ---"
echo "Definen ORDEN, no activan la dependencia"
echo ""
echo "After=network.target:"
echo "  Si network.target esta activo o arrancando,"
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
echo "  Si apache2 esta corriendo, se DETIENE al arrancar esta unidad"
echo "  Si esta unidad esta corriendo, se DETIENE al arrancar apache2"
echo "  Garantiza que ambas no corran simultaneamente"
echo ""
echo "Caso tipico: iptables vs firewalld"
'
```

---

## Parte 2 — Targets y sincronizacion

### Objetivo

Entender los targets como puntos de sincronizacion y la cadena
de boot.

### Paso 2.1: Cadena de boot

```bash
docker compose exec debian-dev bash -c '
echo "=== Cadena de boot ==="
echo ""
echo "local-fs-pre.target"
echo "        │"
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
echo "Los targets son puntos de sincronizacion:"
echo "  sysinit.target  = udev, tmpfiles, journal, swap"
echo "  basic.target    = D-Bus, sockets base, timers base"
echo "  network.target  = interfaces con IP asignada"
echo "  multi-user.target = servidor completo sin GUI"
'
```

### Paso 2.2: network.target vs network-online.target

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

### Paso 2.3: Dependencias en unit files reales

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

## Parte 3 — Verificacion y errores

### Objetivo

Detectar errores de dependencias y entender los patrones
incorrectos mas comunes.

### Paso 3.1: Requires sin After

Antes de ejecutar, predice: que pasa si usas `Requires=` sin
`After=`?

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
echo "# Si necesitas que postgresql este listo → puede fallar"
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
echo "# Arranca postgresql Y espera a que este listo"
echo ""
echo "--- Verificar ---"
systemd-analyze verify /tmp/lab-noafter.service 2>&1 || true
echo ""
systemd-analyze verify /tmp/lab-withafter.service 2>&1 || true
'
```

### Paso 3.2: Dependency loops

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
echo "Solucion: revisar la cadena de After/Before y romper el ciclo"
rm /tmp/lab-loop-a.service /tmp/lab-loop-b.service
'
```

### Paso 3.3: DefaultDependencies

```bash
docker compose exec debian-dev bash -c '
echo "=== DefaultDependencies ==="
echo ""
echo "La mayoria de unidades tiene DefaultDependencies=yes (implicito)"
echo "Esto agrega automaticamente:"
echo "  Requires=sysinit.target"
echo "  After=sysinit.target basic.target"
echo "  Conflicts=shutdown.target"
echo "  Before=shutdown.target"
echo ""
echo "Esto asegura que:"
echo "  - Los servicios arrancan DESPUES de la inicializacion"
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

### Paso 3.4: Ejemplo completo con dependencias

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

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/lab-wants.service /tmp/lab-requires.service
rm -f /tmp/lab-partof.service /tmp/lab-conflicts.service
rm -f /tmp/lab-noafter.service /tmp/lab-withafter.service
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. Las dependencias tienen dos ejes **independientes**: activacion
   (`Wants=`, `Requires=`) y ordenamiento (`After=`, `Before=`).
   `Requires=` sin `After=` arranca en paralelo.

2. `Wants=` es tolerante a fallos (la dependencia puede fallar).
   `Requires=` es estricto (falla si la dependencia falla).
   `BindsTo=` es aun mas estricto (cualquier estado inactivo).

3. `PartOf=` propaga stop/restart pero no start. Ideal para
   componentes auxiliares (workers, sidecars) que deben seguir
   el ciclo de vida del servicio principal.

4. `network.target` = interfaces con IP (para servicios que
   escuchan). `network-online.target` = conectividad real (para
   servicios que conectan). El segundo requiere `Wants=`.

5. `DefaultDependencies=yes` (implicito) agrega automaticamente
   dependencias de sysinit y shutdown. Solo desactivar para
   unidades de early boot.

6. `systemd-analyze verify` detecta dependency loops y errores
   de configuracion en archivos de unidad.
