# Lab — Unidades y tipos

## Objetivo

Explorar los tipos de unidades que gestiona systemd: service, socket,
timer, target, mount, slice, path y swap. Aprender a listar, filtrar
e inspeccionar unidades instaladas, y entender como se relacionan
entre si.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Explorar unidades instaladas

### Objetivo

Listar y filtrar las unidades instaladas en el sistema por tipo.

### Paso 1.1: Listar unit files por tipo

```bash
docker compose exec debian-dev bash -c '
echo "=== Unidades instaladas por tipo ==="
echo ""
echo "--- Servicios (.service) ---"
SERVICES=$(ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l)
echo "Total en /usr/lib/systemd/system/: $SERVICES"
ls /usr/lib/systemd/system/*.service 2>/dev/null | head -10
echo "..."
echo ""
echo "--- Sockets (.socket) ---"
ls /usr/lib/systemd/system/*.socket 2>/dev/null
echo ""
echo "--- Timers (.timer) ---"
ls /usr/lib/systemd/system/*.timer 2>/dev/null
echo ""
echo "--- Targets (.target) ---"
TARGETS=$(ls /usr/lib/systemd/system/*.target 2>/dev/null | wc -l)
echo "Total: $TARGETS"
ls /usr/lib/systemd/system/*.target 2>/dev/null | head -10
echo "..."
'
```

### Paso 1.2: Contar unidades por tipo

Antes de ejecutar, predice: que tipo de unidad sera el mas
numeroso? Y el menos?

```bash
docker compose exec debian-dev bash -c '
echo "=== Conteo por tipo ==="
echo ""
for ext in service socket timer target mount automount slice path swap; do
    COUNT=$(ls /usr/lib/systemd/system/*."$ext" 2>/dev/null | wc -l)
    printf "  %-12s %3d unidades\n" ".$ext" "$COUNT"
done
echo ""
echo "Los .service son la gran mayoria"
echo "Cada tipo tiene un proposito especifico:"
echo "  service = procesos/daemons"
echo "  socket  = activacion por conexion"
echo "  timer   = tareas programadas (reemplazo de cron)"
echo "  target  = agrupacion de unidades"
echo "  mount   = puntos de montaje"
echo "  slice   = control de recursos (cgroups)"
echo "  path    = activacion por filesystem"
echo "  swap    = dispositivos de swap"
'
```

### Paso 1.3: Extensiones y nombres

```bash
docker compose exec debian-dev bash -c '
echo "=== Nombres de unidades ==="
echo ""
echo "El tipo se determina por la extension del archivo:"
echo ""
echo "--- Ejemplo: servicios ---"
ls /usr/lib/systemd/system/systemd-*.service 2>/dev/null | head -5
echo ""
echo "--- Ejemplo: mounts ---"
echo "El nombre refleja la ruta con - en lugar de /:"
ls /usr/lib/systemd/system/*.mount 2>/dev/null | head -5
echo ""
echo "Por ejemplo:"
echo "  /tmp          → tmp.mount"
echo "  /var/log      → var-log.mount"
echo "  /home         → home.mount"
echo ""
echo "--- Ejemplo: targets ---"
ls /usr/lib/systemd/system/multi-user.target \
   /usr/lib/systemd/system/graphical.target \
   /usr/lib/systemd/system/rescue.target \
   /usr/lib/systemd/system/basic.target 2>/dev/null
'
```

---

## Parte 2 — Inspeccionar unidades

### Objetivo

Leer el contenido de archivos de unidad para entender su estructura
interna y las secciones comunes ([Unit], [Service], [Socket], etc.).

### Paso 2.1: Anatomia de un .service

```bash
docker compose exec debian-dev bash -c '
echo "=== Anatomia de un .service ==="
echo ""
# Buscar un servicio representativo
SVC=$(ls /usr/lib/systemd/system/systemd-journald.service 2>/dev/null || \
      ls /usr/lib/systemd/system/cron.service 2>/dev/null | head -1)
if [[ -n "$SVC" ]]; then
    echo "Archivo: $SVC"
    echo ""
    cat "$SVC"
    echo ""
    echo "--- Secciones ---"
    echo "[Unit]    = metadata y dependencias (comun a todos los tipos)"
    echo "[Service] = configuracion especifica del servicio"
    echo "[Install] = como se integra al boot (enable/disable)"
else
    echo "Mostrando estructura tipica de un .service:"
    echo ""
    echo "[Unit]"
    echo "Description=My Service"
    echo "After=network.target"
    echo ""
    echo "[Service]"
    echo "ExecStart=/usr/bin/myapp"
    echo "Restart=on-failure"
    echo ""
    echo "[Install]"
    echo "WantedBy=multi-user.target"
fi
'
```

### Paso 2.2: Anatomia de un .socket

```bash
docker compose exec debian-dev bash -c '
echo "=== Anatomia de un .socket ==="
echo ""
SOCK=$(ls /usr/lib/systemd/system/*.socket 2>/dev/null | head -1)
if [[ -n "$SOCK" ]]; then
    echo "Archivo: $SOCK"
    echo ""
    cat "$SOCK"
    echo ""
    echo "--- Secciones ---"
    echo "[Unit]    = metadata"
    echo "[Socket]  = configuracion del socket (ListenStream, Accept, etc.)"
    echo "[Install] = integracion al boot"
    echo ""
    echo "Un .socket activa un .service cuando llega una conexion"
    echo "El servicio NO consume recursos hasta que se necesita"
else
    echo "No se encontraron archivos .socket"
    echo ""
    echo "Estructura tipica:"
    echo "[Socket]"
    echo "ListenStream=22"
    echo "Accept=yes"
fi
'
```

### Paso 2.3: Anatomia de un .timer

```bash
docker compose exec debian-dev bash -c '
echo "=== Anatomia de un .timer ==="
echo ""
TIMER=$(ls /usr/lib/systemd/system/*.timer 2>/dev/null | head -1)
if [[ -n "$TIMER" ]]; then
    echo "Archivo: $TIMER"
    echo ""
    cat "$TIMER"
    echo ""
    echo "--- Secciones ---"
    echo "[Unit]   = metadata"
    echo "[Timer]  = configuracion del timer (OnCalendar, Persistent, etc.)"
    echo "[Install] = integracion al boot"
    echo ""
    echo "Un .timer activa un .service segun el calendario definido"
    echo "Es el reemplazo moderno de cron"
else
    echo "No se encontraron archivos .timer"
    echo ""
    echo "Estructura tipica:"
    echo "[Timer]"
    echo "OnCalendar=daily"
    echo "Persistent=true"
fi
'
```

### Paso 2.4: Anatomia de un .target

```bash
docker compose exec debian-dev bash -c '
echo "=== Anatomia de un .target ==="
echo ""
echo "--- multi-user.target ---"
cat /usr/lib/systemd/system/multi-user.target 2>/dev/null || \
    echo "No encontrado"
echo ""
echo "--- rescue.target ---"
cat /usr/lib/systemd/system/rescue.target 2>/dev/null || \
    echo "No encontrado"
echo ""
echo "Los targets son simples: solo [Unit] e [Install]"
echo "Su funcion es AGRUPAR otras unidades mediante Wants/Requires"
echo "No tienen seccion de tipo especifico ([Service], [Socket], etc.)"
echo ""
echo "--- Targets principales ---"
echo "poweroff.target   = apagar (runlevel 0)"
echo "rescue.target     = single-user (runlevel 1)"
echo "multi-user.target = servidor sin GUI (runlevel 3)"
echo "graphical.target  = desktop con GUI (runlevel 5)"
echo "reboot.target     = reiniciar (runlevel 6)"
'
```

---

## Parte 3 — Relaciones entre tipos

### Objetivo

Entender como los diferentes tipos de unidades se relacionan:
targets agrupan servicios, sockets activan servicios, timers
disparan servicios.

### Paso 3.1: Targets como agrupadores

```bash
docker compose exec debian-dev bash -c '
echo "=== Targets agrupan servicios ==="
echo ""
echo "Los servicios declaran WantedBy=multi-user.target en [Install]"
echo "Al hacer systemctl enable, se crea un symlink en:"
echo "  /etc/systemd/system/multi-user.target.wants/"
echo ""
echo "--- Symlinks en multi-user.target.wants/ ---"
ls -la /etc/systemd/system/multi-user.target.wants/ 2>/dev/null || \
    echo "(directorio no existe o vacio)"
echo ""
echo "--- Otros directorios .wants/ ---"
find /etc/systemd/system -name "*.wants" -type d 2>/dev/null
echo ""
echo "Cada directorio .wants/ contiene symlinks a las unidades"
echo "que se activan cuando ese target se alcanza"
'
```

### Paso 3.2: Socket activa service

```bash
docker compose exec debian-dev bash -c '
echo "=== Socket → Service ==="
echo ""
echo "Convencion: sshd.socket activa sshd.service"
echo "El nombre del socket y el servicio deben coincidir"
echo ""
echo "--- Buscar pares socket/service ---"
for sock in /usr/lib/systemd/system/*.socket; do
    [[ -f "$sock" ]] || continue
    BASE=$(basename "$sock" .socket)
    SVC="/usr/lib/systemd/system/${BASE}.service"
    if [[ -f "$SVC" ]]; then
        echo "  $(basename "$sock") → ${BASE}.service"
    fi
done
echo ""
echo "Ventajas de activacion por socket:"
echo "  1. El servicio no arranca hasta que alguien se conecta"
echo "  2. Si el servicio crashea, el socket sigue escuchando"
echo "  3. Boot mas rapido (sockets listos inmediatamente)"
'
```

### Paso 3.3: Timer activa service

```bash
docker compose exec debian-dev bash -c '
echo "=== Timer → Service ==="
echo ""
echo "Convencion: backup.timer activa backup.service"
echo ""
echo "--- Buscar pares timer/service ---"
for tmr in /usr/lib/systemd/system/*.timer; do
    [[ -f "$tmr" ]] || continue
    BASE=$(basename "$tmr" .timer)
    SVC="/usr/lib/systemd/system/${BASE}.service"
    if [[ -f "$SVC" ]]; then
        echo "  $(basename "$tmr") → ${BASE}.service"
        echo "    Timer:"
        grep -E "OnCalendar|OnBoot|OnUnitActive" "$tmr" 2>/dev/null | \
            sed "s/^/      /"
    fi
done
echo ""
echo "Los timers reemplazan a cron con ventajas:"
echo "  - Dependencias (After=, Requires=)"
echo "  - Persistent=true (ejecuta al arrancar si se perdio)"
echo "  - Logs integrados en journalctl"
echo "  - Control granular con systemctl"
'
```

### Paso 3.4: Resumen de relaciones

```bash
docker compose exec debian-dev bash -c '
echo "=== Diagrama de relaciones ==="
echo ""
echo "  multi-user.target"
echo "       │"
echo "       ├── Wants/Requires"
echo "       │"
echo "  ┌────┼────┐"
echo "  │    │    │"
echo "  sshd nginx docker"
echo "  .svc .svc .socket ── activa ──▶ docker.service"
echo ""
echo "  apt-daily.timer ── activa ──▶ apt-daily.service"
echo ""
echo "  upload.path ── activa ──▶ upload.service"
echo ""
echo "  system.slice ── controla recursos de ──▶ servicios"
echo ""
echo "Resumen:"
echo "  target  → agrupa unidades"
echo "  socket  → activa un service al recibir conexion"
echo "  timer   → activa un service segun calendario"
echo "  path    → activa un service al detectar cambio en FS"
echo "  slice   → controla recursos (CPU, memoria) de servicios"
echo "  service → el proceso/daemon real"
echo "  mount   → punto de montaje"
echo "  swap    → dispositivo de swap"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. El tipo de unidad se determina por la extension del archivo:
   `.service`, `.socket`, `.timer`, `.target`, `.mount`, `.slice`,
   `.path`, `.swap`. Los `.service` son la mayoria.

2. Cada tipo tiene sus propias secciones: `[Service]`, `[Socket]`,
   `[Timer]`, etc. La seccion `[Unit]` es comun a todos los tipos.

3. Los targets agrupan unidades a traves de `Wants=`/`Requires=`
   y directorios `.wants/`. Multiples targets pueden estar activos
   simultaneamente (a diferencia de los runlevels).

4. Los sockets y timers activan servicios bajo demanda: sockets
   al recibir una conexion, timers segun un calendario. El servicio
   no consume recursos hasta que se necesita.

5. Los nombres de unidades `.mount` reflejan la ruta del punto
   de montaje con `-` en lugar de `/`: `/var/log` se convierte
   en `var-log.mount`.

6. Los slices organizan procesos en cgroups para controlar
   recursos. `system.slice` contiene los servicios del sistema,
   `user.slice` las sesiones de usuario.
