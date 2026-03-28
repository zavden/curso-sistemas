# Lab — at

## Objetivo

Dominar el comando at para ejecucion unica programada: formatos
de tiempo (hora, fecha, expresiones relativas, palabras clave),
crear tareas desde pipe, heredoc y archivo, gestionar la cola
con atq/atrm, inspeccionar tareas con at -c, colas de prioridad,
control de acceso con at.allow/at.deny, la captura automatica
de entorno, y casos de uso comunes.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Sintaxis y creacion

### Objetivo

Entender los formatos de tiempo de at y las formas de crear tareas.

### Paso 1.1: at vs cron

```bash
docker compose exec debian-dev bash -c '
echo "=== at vs cron ==="
echo ""
echo "cron → \"cada dia a las 03:00\" (repetitiva)"
echo "at   → \"manana a las 03:00\" (una sola vez)"
echo ""
echo "--- Verificar instalacion ---"
which at 2>/dev/null && echo "at: instalado" || \
    echo "at: no instalado (paquete: at)"
echo ""
echo "--- Daemon ---"
systemctl is-active atd 2>/dev/null && echo "atd: ACTIVO" || \
    echo "atd: NO ACTIVO (normal en contenedores)"
'
```

### Paso 1.2: Formatos de tiempo

```bash
docker compose exec debian-dev bash -c '
echo "=== Formatos de tiempo de at ==="
echo ""
echo "--- Hora especifica ---"
echo "at 15:30               hoy (o manana si ya paso)"
echo "at 3:30 PM             formato 12 horas"
echo ""
echo "--- Palabras clave ---"
echo "at now                 ahora"
echo "at noon                12:00"
echo "at midnight            00:00"
echo "at teatime             16:00"
echo "at tomorrow            manana (misma hora)"
echo ""
echo "--- Expresiones relativas ---"
echo "at now + 5 minutes     en 5 minutos"
echo "at now + 2 hours       en 2 horas"
echo "at now + 3 days        en 3 dias"
echo "at now + 1 week        en 1 semana"
echo ""
echo "--- Fecha y hora ---"
echo "at 15:00 2026-03-20    ISO (recomendado)"
echo "at 15:00 Mar 20        nombre de mes"
echo "at 15:00 Friday        dia de la semana"
echo "at 15:00 next Friday   proximo viernes"
echo ""
echo "--- Nota ---"
echo "Si la hora ya paso hoy, at la programa para manana."
echo "Unidades validas: minutes, hours, days, weeks"
'
```

### Paso 1.3: Crear tareas

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear tareas con at ==="
echo ""
echo "--- Desde pipe ---"
echo "echo \"/usr/local/bin/backup.sh\" | at 03:00"
echo "echo \"cd /opt && git pull\" | at 22:00"
echo ""
echo "--- Desde heredoc ---"
echo "at 03:00 tomorrow << '\''EOF'\''"
echo "/usr/local/bin/backup.sh"
echo "/usr/local/bin/cleanup.sh"
echo "logger \"Mantenimiento completado\""
echo "EOF"
echo ""
echo "--- Desde archivo (-f) ---"
echo "at -f /tmp/tasks.sh 03:00 tomorrow"
echo ""
echo "--- Modo interactivo ---"
echo "at 14:00 tomorrow"
echo "at> /usr/local/bin/backup.sh"
echo "at> <Ctrl+D>"
echo ""
echo "--- Forzar mail de notificacion ---"
echo "at -m 15:00    enviar mail al completar (aun sin salida)"
'
```

---

## Parte 2 — Gestion de tareas

### Objetivo

Gestionar la cola de at: listar, inspeccionar y eliminar tareas.

### Paso 2.1: Listar tareas (atq)

```bash
docker compose exec debian-dev bash -c '
echo "=== Listar tareas: atq ==="
echo ""
echo "--- Formato ---"
echo "ID   Fecha/hora programada      Cola  Usuario"
echo " 7   Wed Mar 18 14:00:00 2026    a     user"
echo ""
echo "--- Colas ---"
echo "a = cola por defecto de at"
echo "b = cola de batch"
echo "c-z = colas adicionales (mayor letra = menor prioridad)"
echo ""
echo "--- Verificar ---"
if command -v atq &>/dev/null; then
    echo "Cola actual:"
    atq 2>/dev/null || echo "  (sin tareas o atd no activo)"
else
    echo "atq no disponible"
fi
'
```

### Paso 2.2: Inspeccionar y eliminar

```bash
docker compose exec debian-dev bash -c '
echo "=== Inspeccionar y eliminar ==="
echo ""
echo "--- Ver contenido de una tarea ---"
echo "at -c 7"
echo "  Muestra el script completo:"
echo "  - Entorno capturado (variables exportadas)"
echo "  - Directorio de trabajo"
echo "  - umask"
echo "  - Comandos del usuario al final"
echo ""
echo "Solo ver los comandos (omitir preambulo):"
echo "  at -c 7 | tail -5"
echo ""
echo "--- Eliminar tareas ---"
echo "atrm 7           eliminar tarea 7"
echo "atrm 7 8 12      eliminar multiples"
echo ""
echo "--- Alternativas ---"
echo "at -r 7           equivale a atrm 7 (algunos sistemas)"
echo "at -d 7           otra alternativa"
'
```

### Paso 2.3: Colas de prioridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Colas de prioridad ==="
echo ""
echo "at soporta multiples colas (a-z):"
echo ""
echo "| Cola | Nice | Uso                    |"
echo "|------|------|------------------------|"
echo "| a    | 0    | at normal (default)    |"
echo "| b    | 2    | batch                  |"
echo "| c    | 4    | prioridad reducida     |"
echo "| z    | 50   | prioridad muy baja     |"
echo ""
echo "Mayor letra = mayor nice = menor prioridad de CPU"
echo ""
echo "--- Usar una cola especifica ---"
echo "at -q c 15:00    usar cola c (nice 4)"
echo ""
echo "--- Almacenamiento ---"
echo "Las tareas se guardan en:"
echo "  /var/spool/at/ (Debian)"
echo "  /var/spool/atjobs/ (algunos sistemas)"
echo ""
echo "Cada tarea es un archivo con el entorno + comandos."
echo "Las tareas ejecutadas se eliminan automaticamente."
'
```

---

## Parte 3 — Entorno y control de acceso

### Objetivo

Entender la captura de entorno y el control de acceso de at.

### Paso 3.1: Captura de entorno

```bash
docker compose exec debian-dev bash -c '
echo "=== Captura de entorno ==="
echo ""
echo "A diferencia de cron, at captura el entorno actual:"
echo ""
echo "  cd /opt/myapp"
echo "  MY_VAR=\"hello\""
echo "  echo '\''echo \$MY_VAR from \$(pwd)'\'' | at now + 1 minute"
echo ""
echo "at preserva:"
echo "  - El directorio de trabajo actual (pwd)"
echo "  - Todas las variables de entorno"
echo "  - umask"
echo ""
echo "at NO hereda:"
echo "  - La sesion de terminal (no hay TTY)"
echo "  - DISPLAY (no funciona para GUI)"
echo "  - El shell interactivo (usa /bin/sh)"
echo ""
echo "--- Comparacion con cron ---"
echo "| Aspecto        | at             | cron               |"
echo "|----------------|----------------|--------------------|"
echo "| Captura entorno| Si (completo)  | No (PATH minimo)   |"
echo "| Directorio     | pwd actual     | HOME del usuario   |"
echo "| SHELL          | /bin/sh        | /bin/sh            |"
echo "| Variables      | Todas          | Solo las definidas |"
'
```

### Paso 3.2: Control de acceso

```bash
docker compose exec debian-dev bash -c '
echo "=== Control de acceso ==="
echo ""
echo "Dos archivos controlan quien puede usar at:"
echo ""
echo "  /etc/at.allow → si existe, solo estos usuarios"
echo "  /etc/at.deny  → si existe (y no hay at.allow), estos NO"
echo ""
echo "--- Logica ---"
echo "1. Si /etc/at.allow existe → solo los listados pueden usar at"
echo "2. Si /etc/at.allow NO existe pero /etc/at.deny SI:"
echo "   → todos pueden EXCEPTO los listados"
echo "3. Si NINGUNO existe:"
echo "   → solo root (Debian) o todos (RHEL, at.deny vacio)"
echo ""
echo "--- Verificar ---"
echo "/etc/at.allow:"
cat /etc/at.allow 2>/dev/null || echo "  (no existe)"
echo "/etc/at.deny:"
cat /etc/at.deny 2>/dev/null || echo "  (no existe)"
echo ""
echo "root siempre puede usar at."
'
```

### Paso 3.3: Casos de uso

```bash
docker compose exec debian-dev bash -c '
echo "=== Casos de uso de at ==="
echo ""
echo "--- Reinicio programado ---"
echo "echo \"shutdown -r now\" | sudo at now + 2 hours"
echo "  Cancelar: sudo atrm ID"
echo ""
echo "--- Verificacion post-deploy ---"
echo "at now + 10 minutes << '\''EOF'\''"
echo "STATUS=\$(curl -s -o /dev/null -w \"%{http_code}\" http://localhost:8080/health)"
echo "if [ \"\$STATUS\" != \"200\" ]; then"
echo "    echo \"Health check fallo\" | mail -s \"Deploy FAILED\" ops@example.com"
echo "fi"
echo "EOF"
echo ""
echo "--- Limpieza diferida ---"
echo "echo '\''find /tmp/build-* -mtime +1 -delete'\'' | at 04:00 tomorrow"
echo ""
echo "--- Recordatorio ---"
echo "echo '\''echo \"Reunion en 15 min\" | wall'\'' | sudo at 09:45"
'
```

### Paso 3.4: at vs cron vs systemd timers

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparacion de herramientas ==="
echo ""
echo "| Aspecto          | at            | cron       | systemd timer   |"
echo "|------------------|---------------|------------|-----------------|"
echo "| Ejecucion        | Una vez       | Repetitiva | Repetitiva/unica|"
echo "| Precision        | Minuto        | Minuto     | Segundo         |"
echo "| Captura entorno  | Si            | No         | En .service     |"
echo "| Historial        | No            | No         | Si (journal)    |"
echo "| Persistencia     | Si (disco)    | Si         | Si (Persistent) |"
echo "| Usuarios         | Cualquiera    | Cualquiera | Root o user     |"
echo ""
echo "--- Alternativa moderna ---"
echo "systemd-run --on-active=2h /usr/local/bin/task.sh"
echo "  Crea un timer transient que ejecuta en 2 horas"
echo "  Equivale a: echo /usr/local/bin/task.sh | at now + 2 hours"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `at` ejecuta comandos una sola vez en un momento futuro.
   Soporta formatos de tiempo absolutos (15:00, 2026-03-20),
   relativos (now + 2 hours), y palabras clave (noon, tomorrow).

2. Se crean tareas desde pipe, heredoc, archivo (-f) o modo
   interactivo. `atq` lista, `at -c ID` inspecciona, `atrm ID`
   elimina.

3. at captura el entorno completo al crear la tarea (variables,
   pwd, umask) y lo restaura al ejecutar. A diferencia de cron,
   no hay problemas de PATH reducido.

4. Las colas a-z ofrecen diferentes prioridades (nice). La cola
   "a" es la default de at, "b" es batch.

5. Control de acceso: /etc/at.allow (whitelist) tiene precedencia
   sobre /etc/at.deny (blacklist). root siempre puede usar at.

6. `systemd-run --on-active=` es la alternativa moderna a at para
   ejecucion unica programada, con logs en el journal.
