# Lab — Timers vs cron

## Objetivo

Comparar systemd timers con cron: la arquitectura timer+service,
las ventajas sobre cron (journal, dependencias, recursos, Persistent,
precision, seguridad), las desventajas (verbosidad, portabilidad),
la sintaxis OnCalendar con systemd-analyze calendar, AccuracySec,
RandomizedDelaySec, y la tabla comparativa completa.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Arquitectura y ventajas

### Objetivo

Entender como funcionan los timers y sus ventajas sobre cron.

### Paso 1.1: Arquitectura timer + service

```bash
docker compose exec debian-dev bash -c '
echo "=== Arquitectura: timer + service ==="
echo ""
echo "Un timer siempre necesita un service asociado:"
echo ""
echo "  fstrim.timer"
echo "       │"
echo "       │ cuando se cumple la condicion de tiempo"
echo "       ▼"
echo "  fstrim.service"
echo "       │"
echo "       │ ExecStart="
echo "       ▼"
echo "  /usr/sbin/fstrim --all"
echo ""
echo "--- Asociacion por nombre ---"
echo "myapp.timer → activa → myapp.service (automatico)"
echo ""
echo "--- Nombre diferente ---"
echo "[Timer]"
echo "Unit=otro-nombre.service"
echo ""
echo "--- Listar timers activos ---"
systemctl list-timers --all --no-pager 2>&1 | head -10 || \
    echo "(systemctl no disponible como esperado en contenedor)"
'
```

### Paso 1.2: Ventajas sobre cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Ventajas de timers sobre cron ==="
echo ""
echo "1. JOURNAL INTEGRADO"
echo "   cron: salida va a mail (si hay MTA) o se pierde"
echo "   timer: journalctl -u myapp.service (logs completos)"
echo ""
echo "2. DEPENDENCIAS"
echo "   cron: tareas independientes, sin orden"
echo "   timer: After=network-online.target, Requires=postgresql.service"
echo ""
echo "3. CONTROL DE RECURSOS"
echo "   cron: nice/ionice manualmente"
echo "   timer: CPUQuota=50%, MemoryMax=512M, IOWeight=100"
echo ""
echo "4. EJECUCIONES PERDIDAS (Persistent=)"
echo "   cron: si apagado, tarea perdida (necesita anacron)"
echo "   timer: Persistent=true ejecuta al encender"
echo ""
echo "5. PRECISION"
echo "   cron: minuto (minimo)"
echo "   timer: microsegundo (AccuracySec=1us)"
echo ""
echo "6. ALEATORIZACION"
echo "   cron: no hay"
echo "   timer: RandomizedDelaySec=1h (evita thundering herd)"
echo ""
echo "7. SEGURIDAD"
echo "   cron: solo UID del usuario"
echo "   timer: ProtectSystem, PrivateTmp, NoNewPrivileges, etc."
echo ""
echo "8. ESTADO"
echo "   cron: sin forma nativa de saber si se ejecuto"
echo "   timer: systemctl status myapp.timer/.service"
'
```

### Paso 1.3: Desventajas frente a cron

```bash
docker compose exec debian-dev bash -c '
echo "=== Desventajas frente a cron ==="
echo ""
echo "1. VERBOSIDAD"
echo "   cron:  \"0 3 * * * /opt/task.sh\" (una linea)"
echo "   timer: myapp.timer + myapp.service (dos archivos)"
echo ""
echo "2. CURVA DE APRENDIZAJE"
echo "   cron: 5 campos + comando (conocido universalmente)"
echo "   timer: OnCalendar + unit files (sintaxis propia)"
echo ""
echo "3. SIN \"crontab -e\" PARA USUARIOS"
echo "   cron: cualquier usuario edita su crontab"
echo "   timer: user units en ~/.config/systemd/user/"
echo ""
echo "4. PORTABILIDAD"
echo "   cron: cualquier Unix (BSD, macOS, Solaris)"
echo "   timer: solo systemd"
echo ""
echo "5. EDICION RAPIDA"
echo "   cron: crontab -e → cambiar linea → guardar"
echo "   timer: editar archivo → daemon-reload → restart timer"
'
```

---

## Parte 2 — OnCalendar y herramientas

### Objetivo

Dominar la sintaxis OnCalendar y usar systemd-analyze calendar.

### Paso 2.1: Sintaxis OnCalendar

```bash
docker compose exec debian-dev bash -c '
echo "=== OnCalendar — sintaxis ==="
echo ""
echo "Formato: DiaDeLaSemana YYYY-MM-DD HH:MM:SS"
echo "  Cada parte es opcional"
echo ""
echo "--- Ejemplos ---"
echo "OnCalendar=*-*-* 03:00:00         cada dia a las 03:00"
echo "OnCalendar=Mon *-*-* 09:00:00     cada lunes a las 09:00"
echo "OnCalendar=*-*-01 00:00:00        dia 1 de cada mes"
echo ""
echo "--- Comodines ---"
echo "*            cualquier valor"
echo "Mon..Fri     rango (lun a vie)"
echo "Mon,Wed,Fri  lista"
echo "00/2         cada 2 (intervalo)"
echo ""
echo "--- Expresiones utiles ---"
echo "minutely     = *-*-* *:*:00"
echo "hourly       = *-*-* *:00:00"
echo "daily        = *-*-* 00:00:00"
echo "weekly       = Mon *-*-* 00:00:00"
echo "monthly      = *-*-01 00:00:00"
echo "yearly       = *-01-01 00:00:00"
echo "quarterly    = *-01,04,07,10-01 00:00:00"
'
```

### Paso 2.2: systemd-analyze calendar

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze calendar ==="
echo ""
echo "Verifica cuando se ejecutaria una expresion OnCalendar."
echo ""
echo "--- Probar expresiones ---"
echo ""
echo "1. Cada dia a las 03:00:"
systemd-analyze calendar "daily" --iterations=3 2>&1 || \
    echo "(systemd-analyze no disponible)"
echo ""
echo "2. Lunes a viernes a las 09:00:"
systemd-analyze calendar "Mon..Fri *-*-* 09:00" --iterations=3 2>&1 || true
echo ""
echo "3. Cada 15 minutos:"
systemd-analyze calendar "*-*-* *:00/15:00" --iterations=5 2>&1 || true
echo ""
echo "4. Dia 1 y 15 de cada mes:"
systemd-analyze calendar "*-*-01,15 00:00:00" --iterations=5 2>&1 || true
'
```

### Paso 2.3: Conversion cron → OnCalendar

```bash
docker compose exec debian-dev bash -c '
echo "=== Conversion cron → OnCalendar ==="
echo ""
echo "| Cron             | OnCalendar                       |"
echo "|------------------|----------------------------------|"
echo "| * * * * *        | *-*-* *:*:00 (o minutely)        |"
echo "| 0 * * * *        | *-*-* *:00:00 (o hourly)         |"
echo "| 0 0 * * *        | *-*-* 00:00:00 (o daily)         |"
echo "| 0 0 * * 0        | Sun *-*-* 00:00:00 (o weekly)    |"
echo "| 0 0 1 * *        | *-*-01 00:00:00 (o monthly)      |"
echo "| */5 * * * *      | *-*-* *:00/5:00                  |"
echo "| 0 9-17 * * 1-5   | Mon..Fri *-*-* 09..17:00:00      |"
echo "| 30 2 * * *       | *-*-* 02:30:00                   |"
echo ""
echo "--- Verificar una conversion ---"
echo "Cron: 0 6-22/2 * * * (cada 2h, 06:00-22:00)"
echo "OnCalendar: *-*-* 06/2..22:00:00"
systemd-analyze calendar "*-*-* 06..22/2:00:00" --iterations=5 2>&1 || true
'
```

### Paso 2.4: AccuracySec y RandomizedDelaySec

```bash
docker compose exec debian-dev bash -c '
echo "=== AccuracySec ==="
echo ""
echo "systemd agrupa ejecuciones cercanas para ahorrar wake-ups:"
echo ""
echo "AccuracySec=1min (default)"
echo "  Tarea de 03:00 puede ejecutarse entre 03:00 y 03:01"
echo ""
echo "AccuracySec=1s    precision de 1 segundo"
echo "AccuracySec=1us   precision de microsegundo"
echo "AccuracySec=1h    puede desviarse 1 hora (ahorro energia)"
echo ""
echo "=== RandomizedDelaySec ==="
echo ""
echo "Agrega delay aleatorio para evitar thundering herd:"
echo ""
echo "RandomizedDelaySec=30min"
echo "  Tarea de 03:00 ejecuta entre 03:00 y 03:30 (aleatorio)"
echo ""
echo "Util para:"
echo "  Actualizaciones desde repositorio central"
echo "  Backups a servidor NFS compartido"
echo "  Reportes a API con rate limit"
'
```

---

## Parte 3 — Comparacion completa

### Objetivo

Comparar cron, anacron y systemd timers con una tabla completa.

### Paso 3.1: Tabla comparativa

```bash
docker compose exec debian-dev bash -c '
echo "=== Tabla comparativa ==="
echo ""
echo "| Aspecto            | cron      | anacron   | systemd timer  |"
echo "|--------------------|-----------|-----------|----------------|"
echo "| Precision minima   | 1 minuto  | 1 dia     | 1 microsegundo |"
echo "| Tareas perdidas    | Perdidas  | Ejecuta   | Persistent=    |"
echo "| Logs               | Mail      | syslog    | journal        |"
echo "| Dependencias       | No        | No        | Si (After=)    |"
echo "| Recursos (cgroups) | No        | No        | Si (CPUQuota=) |"
echo "| Seguridad          | Solo UID  | Solo root | Sandboxing     |"
echo "| Estado             | crontab -l| anacron -d| systemctl      |"
echo "| Usuarios           | crontab -e| Solo root | User units     |"
echo "| Portabilidad       | Universal | Linux     | Solo systemd   |"
echo "| Complejidad        | Baja      | Baja      | Media          |"
echo "| Archivos           | 1 linea   | 1 linea   | 2 archivos     |"
echo "| Aleatorizacion     | No        | RANDOM_D  | Randomized*    |"
'
```

### Paso 3.2: Timers del sistema

```bash
docker compose exec debian-dev bash -c '
echo "=== Timers del sistema ==="
echo ""
echo "--- Listar todos ---"
systemctl list-timers --all --no-pager 2>&1 || \
    echo "(list-timers no disponible)"
echo ""
echo "--- Examinar un timer real ---"
for timer in fstrim.timer logrotate.timer apt-daily.timer; do
    if systemctl cat "$timer" &>/dev/null 2>&1; then
        echo "--- $timer ---"
        systemctl cat "$timer" 2>/dev/null | grep -E "^(OnCalendar|Persistent|AccuracySec|Randomized)"
        echo ""
    fi
done
echo ""
echo "--- Timer tipico ---"
echo "[Timer]"
echo "OnCalendar=weekly"
echo "AccuracySec=1h"
echo "Persistent=true"
echo ""
echo "[Install]"
echo "WantedBy=timers.target"
'
```

### Paso 3.3: Cuando usar cada herramienta

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando usar cada herramienta ==="
echo ""
echo "--- cron ---"
echo "  Tareas simples y repetitivas"
echo "  Entornos sin systemd"
echo "  Cuando la portabilidad importa"
echo "  Cuando se necesita edicion rapida (crontab -e)"
echo ""
echo "--- anacron ---"
echo "  Laptops y escritorios que se apagan"
echo "  Tareas diarias/semanales/mensuales sin hora exacta"
echo "  Scripts en /etc/cron.daily/"
echo ""
echo "--- systemd timers ---"
echo "  Servidores con systemd"
echo "  Cuando se necesitan dependencias o sandboxing"
echo "  Cuando se necesita Persistent= para intervalos < 1 dia"
echo "  Cuando se necesitan logs en el journal"
echo "  Cuando se necesita precision sub-minuto"
echo ""
echo "--- at ---"
echo "  Ejecucion unica programada"
echo "  systemd-run --on-active= como alternativa"
echo ""
echo "--- batch ---"
echo "  Tareas pesadas cuando hay recursos libres"
echo "  nice -n 19 ionice -c 3 como alternativa"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Un timer necesita un service asociado (mismo nombre o Unit=).
   El timer define cuando, el service define que ejecutar.

2. Las ventajas principales sobre cron: logs en journal,
   dependencias, control de recursos, Persistent=, precision
   sub-minuto, sandboxing, y estado con systemctl.

3. Las desventajas: verbosidad (2 archivos vs 1 linea),
   portabilidad (solo systemd), y curva de aprendizaje.

4. OnCalendar usa formato `DiaSemana YYYY-MM-DD HH:MM:SS`.
   `systemd-analyze calendar --iterations=N` verifica expresiones.

5. AccuracySec controla la precision (default 1min).
   RandomizedDelaySec evita thundering herd en multiples maquinas.

6. cron para simplicidad y portabilidad, anacron para equipos que
   se apagan, timers para servidores con systemd que necesitan
   control avanzado.
