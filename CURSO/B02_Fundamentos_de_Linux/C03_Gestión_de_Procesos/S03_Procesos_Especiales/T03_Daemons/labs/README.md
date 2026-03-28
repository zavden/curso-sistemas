# Lab — Daemons

## Objetivo

Entender daemons: identificar procesos sin terminal, inspeccionar
sus caracteristicas via /proc, y comparar el metodo clasico
(doble fork + PID file) con el moderno (systemd Type=simple).

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Identificar daemons

### Objetivo

Distinguir daemons de procesos interactivos usando ps.

### Paso 1.1: Procesos sin terminal (TTY=?)

```bash
docker compose exec debian-dev bash -c '
echo "=== Procesos sin terminal (candidatos a daemon) ==="
ps -eo pid,ppid,tty,user,cmd | awk "NR==1 || \$3 == \"?\"" | head -15
echo "..."
echo ""
echo "TTY = ?  significa sin terminal de control"
echo "Estos son daemons o procesos en background"
echo ""
echo "=== Procesos CON terminal ==="
ps -eo pid,ppid,tty,user,cmd | awk "NR==1 || \$3 != \"?\"" | head -10
echo "(estos tienen una terminal asociada — no son daemons)"
'
```

### Paso 1.2: Contar daemons vs interactivos

Antes de ejecutar, predice: en un contenedor, habra mas procesos
con terminal o sin terminal?

```bash
docker compose exec debian-dev bash -c '
echo "=== Conteo ==="
TOTAL=$(ps -e --no-headers | wc -l)
NO_TTY=$(ps -eo tty --no-headers | grep -c "?")
WITH_TTY=$((TOTAL - NO_TTY))

echo "Total:         $TOTAL"
echo "Sin terminal:  $NO_TTY"
echo "Con terminal:  $WITH_TTY"
'
```

### Paso 1.3: Session leaders

```bash
docker compose exec debian-dev bash -c '
echo "=== Session leaders (SID == PID) ==="
ps -eo pid,sid,tty,cmd | awk "NR==1 || (\$1 == \$2 && \$3 == \"?\")" | head -10
echo ""
echo "Un daemon tipico es session leader (SID == PID)"
echo "Esto significa que creo su propia sesion (via setsid)"
echo "y no tiene terminal de control"
'
```

### Paso 1.4: Caracteristicas de un daemon

```bash
docker compose exec debian-dev bash -c '
echo "=== 5 caracteristicas de un daemon ==="
echo ""
echo "1. TTY = ?      (sin terminal de control)"
echo "2. PPID = 1     (hijo de init/PID 1)"
echo "3. SID = PID    (session leader)"
echo "4. CWD = /      (no bloquear mount points)"
echo "5. fd 0,1,2     (redirigidos a /dev/null o logs)"
echo ""
echo "No todos los procesos sin TTY son daemons"
echo "(procesos en background con & tambien tienen TTY=?)"
echo "Un daemon real cumple todas o la mayoria de estas"
'
```

---

## Parte 2 — Inspeccionar un daemon

### Objetivo

Examinar un proceso daemon a traves de /proc.

### Paso 2.1: Encontrar un daemon disponible

```bash
docker compose exec debian-dev bash -c '
echo "=== Daemons disponibles en el contenedor ==="
ps -eo pid,ppid,tty,user,cmd | awk "\$3 == \"?\" && \$1 > 1" | head -10
echo ""
echo "=== Buscando daemons conocidos ==="
for d in cron sshd rsyslogd; do
    PID=$(pgrep -x $d 2>/dev/null | head -1)
    if [ -n "$PID" ]; then
        echo "$d encontrado: PID $PID"
    fi
done
'
```

### Paso 2.2: Inspeccionar via /proc

```bash
docker compose exec debian-dev bash -c '
echo "=== Inspeccionar PID 1 como daemon ==="
PID=1

echo "--- Ejecutable ---"
readlink /proc/$PID/exe 2>/dev/null || echo "(no disponible)"

echo ""
echo "--- CWD (directorio de trabajo) ---"
readlink /proc/$PID/cwd 2>/dev/null || echo "(no disponible)"
echo "(un daemon clasico tiene cwd = /)"

echo ""
echo "--- File descriptors ---"
ls -la /proc/$PID/fd/ 2>/dev/null | head -5
echo "(los fd 0,1,2 tipicamente apuntan a /dev/null o sockets)"

echo ""
echo "--- TTY ---"
ps -o tty= -p $PID
echo "(? = sin terminal)"

echo ""
echo "--- Session ID ---"
ps -o pid,sid -p $PID
echo "(si SID == PID → es session leader)"
'
```

### Paso 2.3: Crear un proceso tipo daemon

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear un proceso con caracteristicas de daemon ==="
setsid sleep 300 &
sleep 0.5
DPID=$(pgrep -nf "sleep 300")

echo "PID del daemon simulado: $DPID"
echo ""
echo "--- Verificar caracteristicas ---"
echo "PPID:     $(ps -o ppid= -p $DPID)"
echo "TTY:      $(ps -o tty= -p $DPID)"
echo "SID:      $(ps -o sid= -p $DPID)"
echo "PID:      $DPID"
echo "SID==PID: $([ "$(ps -o sid= -p $DPID | tr -d " ")" = "$DPID" ] && echo "si (session leader)" || echo "no")"

kill $DPID 2>/dev/null
wait 2>/dev/null
'
```

`setsid` crea una nueva sesion y desasocia el proceso de la
terminal actual.

---

## Parte 3 — Doble fork vs systemd

### Objetivo

Comparar el metodo clasico de daemonizacion con el moderno.

### Paso 3.1: El doble fork (metodo clasico)

```bash
docker compose exec debian-dev bash -c '
echo "=== Doble fork: pasos ==="
echo ""
echo "Paso 1: Primer fork"
echo "  padre sale → hijo queda huerfano → adoptado por init"
echo ""
echo "Paso 2: setsid()"
echo "  hijo crea nueva sesion"
echo "  se desconecta del terminal"
echo ""
echo "Paso 3: Segundo fork"
echo "  session leader sale → nieto queda"
echo "  nieto NO es session leader → no puede readquirir terminal"
echo ""
echo "Paso 4: Configuracion"
echo "  chdir(\"/\")     — no bloquear mount points"
echo "  umask(0)        — no heredar umask"
echo "  close(0,1,2)    — cerrar stdin/stdout/stderr"
echo "  open(/dev/null) — redirigir"
echo "  write PID file  — para que otros lo encuentren"
echo ""
echo "=== Resultado: un daemon limpio ==="
echo "Sin terminal, sin sesion heredada, sin fds heredados"
'
```

### Paso 3.2: El metodo moderno (systemd)

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd: el daemon ejecuta en foreground ==="
echo ""
echo "El daemon moderno NO necesita:"
echo "  - fork()"
echo "  - setsid()"
echo "  - chdir()"
echo "  - close fds"
echo "  - PID file"
echo "  - Manejo de logs propio"
echo ""
echo "systemd se encarga de TODO:"
echo "  - Lanzarlo en background"
echo "  - Redirigir stdout/stderr al journal"
echo "  - Rastrear su PID (via cgroup, no PID file)"
echo "  - Reiniciarlo si muere (Restart=on-failure)"
echo "  - Enviarle senales (SIGTERM para stop)"
echo "  - Limitar sus recursos (MemoryMax, CPUQuota)"
echo ""
echo "=== Unit file minimo ==="
echo "[Unit]"
echo "Description=My App"
echo ""
echo "[Service]"
echo "Type=simple"
echo "ExecStart=/usr/bin/myapp"
echo "Restart=on-failure"
echo ""
echo "[Install]"
echo "WantedBy=multi-user.target"
'
```

### Paso 3.3: Tipos de servicio en systemd

```bash
docker compose exec debian-dev bash -c '
echo "=== Type= en unit files ==="
printf "%-12s %-15s %-15s %s\n" "Type" "Fork?" "Listo cuando" "Uso"
printf "%-12s %-15s %-15s %s\n" "----------" "-------------" "-------------" "---"
printf "%-12s %-15s %-15s %s\n" "simple" "No (default)" "Inmediatamente" "Apps modernas"
printf "%-12s %-15s %-15s %s\n" "exec" "No" "exec() exitoso" "Apps modernas"
printf "%-12s %-15s %-15s %s\n" "notify" "No" "sd_notify()" "Apps avanzadas"
printf "%-12s %-15s %-15s %s\n" "forking" "Si (doble fork)" "Padre sale" "Legacy"
printf "%-12s %-15s %-15s %s\n" "oneshot" "N/A" "Proceso termina" "Scripts setup"
echo ""
echo "Para daemons nuevos: Type=simple (el daemon no hace fork)"
echo "Para daemons legacy: Type=forking + PIDFile="
'
```

### Paso 3.4: PID files — problema del pasado

```bash
docker compose exec debian-dev bash -c '
echo "=== PID files: por que son problematicos ==="
echo ""
echo "Clasico: el daemon escribe su PID en /var/run/daemon.pid"
echo ""
echo "Problemas:"
echo "  1. Stale PID file: daemon murio pero el archivo quedo"
echo "     → kill \$(cat /var/run/daemon.pid) mata otro proceso"
echo ""
echo "  2. PID reuse: el kernel reutiliza PIDs"
echo "     → el PID del archivo puede ser de otro proceso"
echo ""
echo "  3. Race condition: leer PID y enviar senal no es atomico"
echo ""
echo "systemd no necesita PID files:"
echo "  - Usa cgroups para rastrear procesos"
echo "  - Sabe exactamente que PIDs pertenecen a cada servicio"
echo "  - Puede matar un servicio y TODOS sus hijos"
echo ""
echo "=== PID files que existen en este contenedor ==="
find /var/run -name "*.pid" 2>/dev/null || echo "(ninguno)"
'
```

### Paso 3.5: Comparacion en Debian vs AlmaLinux

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian: PID 1 ==="
ps -p 1 -o pid,user,cmd
echo ""
echo "=== Debian: /sbin/init ==="
ls -la /sbin/init 2>/dev/null || echo "/sbin/init no existe"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: PID 1 ==="
ps -p 1 -o pid,user,cmd
echo ""
echo "=== AlmaLinux: /sbin/init ==="
ls -la /sbin/init 2>/dev/null || echo "/sbin/init no existe"
'
```

En contenedores, PID 1 no es systemd. En hosts reales,
`/sbin/init` es un enlace a systemd en ambas distribuciones
modernas.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Un **daemon** ejecuta en background sin terminal (TTY=?),
   tipicamente como session leader (SID=PID), con PPID=1,
   cwd=/, y fds redirigidos.

2. El **doble fork** clasico crea un daemon limpio: fork+setsid
   +fork+chdir+close+PID file. Es complejo y propenso a errores.

3. **systemd** elimina la necesidad de daemonizacion: el daemon
   ejecuta en foreground y systemd gestiona background, logs,
   restart, y rastreo via cgroups.

4. **Type=simple** (default) es para daemons modernos que no
   hacen fork. **Type=forking** es para daemons legacy.

5. Los **PID files** son problematicos (stale, reuse, race
   conditions). systemd usa cgroups en su lugar.

6. `setsid` crea una nueva sesion y es la herramienta minima
   para simular un daemon desde la shell.
