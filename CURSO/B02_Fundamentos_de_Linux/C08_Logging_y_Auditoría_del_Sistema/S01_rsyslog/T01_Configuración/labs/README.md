# Lab — Configuración de rsyslog

## Objetivo

Entender rsyslog: el modelo syslog (facility, priority, action),
la configuracion en /etc/rsyslog.conf, modulos de input y output,
reglas de enrutamiento (facility.priority accion), operadores de
prioridad (.= .! .none), drop-ins en /etc/rsyslog.d/, el guion (-)
para escritura buffered, logger para enviar mensajes, verificacion
con rsyslogd -N1, y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Modelo syslog y configuracion

### Objetivo

Entender los tres componentes de un mensaje syslog y la estructura
de rsyslog.conf.

### Paso 1.1: Que es rsyslog

```bash
docker compose exec debian-dev bash -c '
echo "=== rsyslog ==="
echo ""
echo "rsyslog recibe mensajes de log y los enruta a destinos:"
echo ""
echo "  Aplicaciones ── syslog() ──┐"
echo "  Kernel ── /dev/kmsg ───────┤"
echo "  journald ── ForwardTo... ──┼──→ rsyslogd ──→ /var/log/syslog"
echo "  Remoto ── TCP/UDP ─────────┘                  /var/log/auth.log"
echo "                                                 servidor remoto"
echo ""
echo "--- Estado del servicio ---"
systemctl is-active rsyslog 2>/dev/null || echo "(rsyslog no activo)"
echo ""
echo "--- Version ---"
rsyslogd -v 2>&1 | head -2
echo ""
echo "--- Archivos de log creados por rsyslog ---"
ls -la /var/log/syslog /var/log/auth.log /var/log/kern.log \
    /var/log/daemon.log 2>/dev/null || \
    ls -la /var/log/messages /var/log/secure 2>/dev/null || \
    echo "(archivos de log no encontrados)"
'
```

### Paso 1.2: Facility (quien genera el log)

```bash
docker compose exec debian-dev bash -c '
echo "=== Facility ==="
echo ""
echo "Identifica el ORIGEN del mensaje:"
echo ""
echo "| Facility   | Num | Uso                        |"
echo "|------------|-----|----------------------------|"
echo "| kern       | 0   | Kernel                     |"
echo "| user       | 1   | Programas de usuario       |"
echo "| mail       | 2   | Sistema de correo          |"
echo "| daemon     | 3   | Daemons del sistema        |"
echo "| auth       | 4   | Autenticacion (login, ssh) |"
echo "| syslog     | 5   | rsyslog mismo              |"
echo "| lpr        | 6   | Impresion                  |"
echo "| cron       | 9   | cron                       |"
echo "| authpriv   | 10  | Autenticacion privada      |"
echo "| local0-7   | 16-23 | Uso personalizado        |"
echo ""
echo "local0-7 son para tus aplicaciones:"
echo "  local0 = mi app web"
echo "  local1 = mi base de datos custom"
echo "  etc."
'
```

### Paso 1.3: Priority (gravedad del mensaje)

```bash
docker compose exec debian-dev bash -c '
echo "=== Priority (severidad) ==="
echo ""
echo "De mas severa a menos:"
echo ""
echo "| Priority | Num | Significado               |"
echo "|----------|-----|---------------------------|"
echo "| emerg    | 0   | Sistema inutilizable      |"
echo "| alert    | 1   | Accion inmediata necesaria |"
echo "| crit     | 2   | Condicion critica         |"
echo "| err      | 3   | Error                     |"
echo "| warning  | 4   | Advertencia               |"
echo "| notice   | 5   | Normal pero significativo |"
echo "| info     | 6   | Informacional             |"
echo "| debug    | 7   | Depuracion                |"
echo ""
echo "--- Regla clave ---"
echo "  Cuando especificas una prioridad en una regla,"
echo "  incluye ESA prioridad y todas las MAS SEVERAS:"
echo ""
echo "  auth.warning captura:"
echo "    warning + err + crit + alert + emerg"
echo "    (NO captura notice, info, debug)"
'
```

### Paso 1.4: Action (que hacer con el mensaje)

```bash
docker compose exec debian-dev bash -c '
echo "=== Action ==="
echo ""
echo "Destinos posibles para los mensajes:"
echo ""
echo "| Accion                    | Descripcion               |"
echo "|---------------------------|---------------------------|"
echo "| /var/log/syslog           | Escribir en archivo       |"
echo "| -/var/log/syslog          | Archivo sin fsync (rapido)|"
echo "| /dev/console              | Enviar a la consola       |"
echo "| @logserver:514            | UDP a servidor remoto     |"
echo "| @@logserver:514           | TCP a servidor remoto     |"
echo "| :omusql:*                 | Base de datos             |"
echo "| stop                      | Descartar el mensaje      |"
echo "| *                         | Todos los terminales      |"
echo ""
echo "--- Formato de regla ---"
echo "  facility.priority    action"
echo ""
echo "  auth.info            /var/log/auth.log"
echo "  kern.*               /var/log/kern.log"
echo "  *.emerg              :omusql:*"
'
```

### Paso 1.5: Estructura de rsyslog.conf

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/rsyslog.conf ==="
echo ""
echo "Estructura tipica:"
echo "  1. Modulos (modules)"
echo "  2. Configuracion global (global directives)"
echo "  3. Templates"
echo "  4. Reglas (rules)"
echo "  5. Include de archivos adicionales"
echo ""
echo "--- Contenido actual ---"
if [[ -f /etc/rsyslog.conf ]]; then
    echo ""
    echo "Modulos cargados:"
    grep -E "^\\\$ModLoad|^module\(load" /etc/rsyslog.conf 2>/dev/null | head -10
    echo ""
    echo "Directivas globales:"
    grep -E "^\\\$|^global\(" /etc/rsyslog.conf 2>/dev/null | head -10
    echo ""
    echo "Include:"
    grep -i "include" /etc/rsyslog.conf 2>/dev/null
else
    echo "(rsyslog.conf no encontrado)"
fi
'
```

### Paso 1.6: Modulos de rsyslog

```bash
docker compose exec debian-dev bash -c '
echo "=== Modulos de rsyslog ==="
echo ""
echo "--- Input (recibir logs) ---"
echo "  imuxsock    socket Unix /dev/log (apps locales)"
echo "  imklog      logs del kernel (/dev/kmsg)"
echo "  imjournal   leer del journal (RHEL)"
echo "  imtcp       recibir por TCP"
echo "  imudp       recibir por UDP"
echo ""
echo "--- Output (enviar logs) ---"
echo "  omfile      escribir a archivos (default)"
echo "  omfwd       reenviar a otro servidor (default)"
echo "  ommysql     escribir en MySQL"
echo ""
echo "--- Debian vs RHEL ---"
echo "  Debian usa imuxsock (recibe del socket /dev/log)"
echo "  RHEL usa imjournal (lee directamente del journal)"
echo ""
echo "--- Modulos cargados en este sistema ---"
grep -rhE "ModLoad|module\(load" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | sort -u || echo "(sin modulos encontrados)"
'
```

---

## Parte 2 — Reglas y operadores

### Objetivo

Entender las reglas de enrutamiento, operadores y drop-ins.

### Paso 2.1: Reglas basicas

```bash
docker compose exec debian-dev bash -c '
echo "=== Reglas de enrutamiento ==="
echo ""
echo "Formato: facility.priority    accion"
echo ""
echo "--- Ejemplos ---"
echo "auth.info             /var/log/auth.log"
echo "  Auth con prioridad info o superior → auth.log"
echo ""
echo "kern.*                /var/log/kern.log"
echo "  Todo del kernel (cualquier prioridad) → kern.log"
echo ""
echo "*.emerg               :omusql:*"
echo "  Emergencias de cualquier facility → todos los terminales"
echo ""
echo "--- Multiples facilities (coma) ---"
echo "auth,authpriv.*       /var/log/auth.log"
echo "  auth Y authpriv, cualquier prioridad"
echo ""
echo "--- Multiples selectores (punto y coma) ---"
echo "kern.warning;mail.err /var/log/important.log"
echo "  kernel warnings+ O mail errors+"
'
```

### Paso 2.2: Operadores de prioridad

```bash
docker compose exec debian-dev bash -c '
echo "=== Operadores de prioridad ==="
echo ""
echo "--- .prioridad (esta y superiores) ---"
echo "mail.warning          /var/log/mail.warn"
echo "  Captura: warning + err + crit + alert + emerg"
echo ""
echo "--- .=prioridad (solo esta exacta) ---"
echo "mail.=info            /var/log/mail.info"
echo "  Captura: SOLO info, no warning ni superior"
echo ""
echo "--- .none (no capturar nada) ---"
echo "*.*;auth.none         /var/log/syslog"
echo "  Todo EXCEPTO auth → syslog"
echo ""
echo "--- Reglas reales de este sistema ---"
echo ""
if [[ -f /etc/rsyslog.d/50-default.conf ]]; then
    echo "=== /etc/rsyslog.d/50-default.conf ==="
    grep -v "^#" /etc/rsyslog.d/50-default.conf 2>/dev/null | grep -v "^$"
elif [[ -f /etc/rsyslog.conf ]]; then
    echo "=== Reglas en /etc/rsyslog.conf ==="
    grep -E "^\S+\.\S+\s" /etc/rsyslog.conf 2>/dev/null | head -15
fi
'
```

### Paso 2.3: Reglas tipicas Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Reglas tipicas — Debian ==="
echo ""
echo "auth,authpriv.*                 /var/log/auth.log"
echo "  Autenticacion → auth.log"
echo ""
echo "*.*;auth,authpriv.none          -/var/log/syslog"
echo "  Todo excepto auth → syslog (con - para buffered)"
echo ""
echo "kern.*                          -/var/log/kern.log"
echo "  Kernel → kern.log"
echo ""
echo "mail.*                          -/var/log/mail.log"
echo "  Correo → mail.log"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== Reglas tipicas — RHEL ==="
echo ""
echo "*.info;mail.none;authpriv.none;cron.none   /var/log/messages"
echo "  Todo info+ excepto mail, auth y cron → messages"
echo ""
echo "authpriv.*                                  /var/log/secure"
echo "  Auth → secure"
echo ""
echo "mail.*                                      -/var/log/maillog"
echo "  Correo → maillog"
echo ""
echo "cron.*                                      /var/log/cron"
echo "  Cron → cron"
echo ""
echo "--- Verificar reglas reales ---"
grep -E "^\S+\.\S+\s" /etc/rsyslog.conf 2>/dev/null | grep -v "^#" | head -10
'
```

### Paso 2.4: Drop-ins /etc/rsyslog.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== Drop-ins /etc/rsyslog.d/ ==="
echo ""
echo "rsyslog incluye archivos de un directorio drop-in."
echo "Se procesan en orden alfabetico."
echo ""
echo "--- Convencion de numeracion ---"
echo "  00-19: modulos y config global"
echo "  20-49: reglas de aplicaciones"
echo "  50-79: reglas por defecto del sistema"
echo "  80-99: reglas finales (catch-all)"
echo ""
echo "--- Archivos en este sistema ---"
ls -la /etc/rsyslog.d/ 2>/dev/null || echo "(directorio no encontrado)"
echo ""
echo "--- Include en rsyslog.conf ---"
grep -i "include" /etc/rsyslog.conf 2>/dev/null
echo ""
echo "--- Contenido de cada drop-in ---"
for f in /etc/rsyslog.d/*.conf; do
    [[ -f "$f" ]] || continue
    echo ""
    echo "=== $(basename $f) ==="
    grep -v "^#" "$f" | grep -v "^$" | head -10
done
'
```

### Paso 2.5: El guion (-) antes de archivos

```bash
docker compose exec debian-dev bash -c '
echo "=== El guion (-) ==="
echo ""
echo "--- Sin guion (con sync/fsync) ---"
echo "kern.*    /var/log/kern.log"
echo "  Cada escritura se sincroniza a disco"
echo "  Mas seguro: si crashea, los logs estan en disco"
echo "  Mas lento: cada mensaje causa I/O"
echo ""
echo "--- Con guion (buffered) ---"
echo "kern.*    -/var/log/kern.log"
echo "  El kernel bufferea las escrituras"
echo "  Mas rapido: menos I/O"
echo "  Riesgo: si crashea, ultimos logs pueden perderse"
echo ""
echo "--- Recomendacion ---"
echo "  Sin guion: logs criticos (auth, emerg)"
echo "  Con guion: logs de alto volumen (mail, debug)"
echo ""
echo "--- Buscar en la config actual ---"
echo "Con guion (buffered):"
grep -rh "^\S.*\s\+-/" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | head -5 || echo "  (ninguno)"
echo ""
echo "Sin guion (sync):"
grep -rhE "^\S.*\s+/var/log" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | \
    grep -v "^#" | grep -v "\s\+-/" | head -5 || echo "  (ninguno)"
'
```

---

## Parte 3 — logger y diagnostico

### Objetivo

Enviar mensajes con logger, verificar la configuracion y comparar
Debian vs RHEL.

### Paso 3.1: logger — enviar mensajes a syslog

```bash
docker compose exec debian-dev bash -c '
echo "=== logger ==="
echo ""
echo "logger envia mensajes a syslog desde la linea de comandos."
echo ""
echo "--- Basico ---"
logger "Mensaje de prueba desde el lab"
echo "Enviado: logger \"Mensaje de prueba desde el lab\""
echo ""
sleep 1
echo "--- Verificar ---"
grep "Mensaje de prueba desde el lab" /var/log/syslog 2>/dev/null | tail -1 || \
    grep "Mensaje de prueba desde el lab" /var/log/messages 2>/dev/null | tail -1 || \
    echo "(mensaje no encontrado en logs)"
echo ""
echo "--- Con facility y prioridad ---"
logger -p local0.info "myapp: servicio iniciado"
echo "Enviado: logger -p local0.info \"myapp: servicio iniciado\""
echo ""
echo "--- Con tag ---"
logger -t backup "backup completado"
echo "Enviado: logger -t backup \"backup completado\""
echo ""
sleep 1
grep "backup" /var/log/syslog 2>/dev/null | tail -1 || \
    grep "backup" /var/log/messages 2>/dev/null | tail -1 || \
    echo "(mensaje no encontrado)"
'
```

### Paso 3.2: logger con diferentes prioridades

```bash
docker compose exec debian-dev bash -c '
echo "=== logger con prioridades ==="
echo ""
echo "Enviando mensajes con diferentes prioridades:"
echo ""
for prio in emerg alert crit err warning notice info debug; do
    logger -t lab-test -p "user.$prio" "Prueba prioridad: $prio"
    echo "  Enviado: user.$prio"
done
echo ""
sleep 1
echo "--- Donde aparecieron ---"
echo ""
for logfile in /var/log/syslog /var/log/messages /var/log/auth.log /var/log/secure; do
    [[ -f "$logfile" ]] || continue
    COUNT=$(grep -c "lab-test.*Prueba prioridad" "$logfile" 2>/dev/null || echo 0)
    [[ "$COUNT" -gt 0 ]] && echo "  $logfile: $COUNT mensajes"
done
echo ""
echo "Nota: no todos los niveles se registran en todos los archivos."
echo "Depende de las reglas configuradas."
'
```

### Paso 3.3: Verificar configuracion

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar configuracion ==="
echo ""
echo "--- rsyslogd -N1 (validar sintaxis) ---"
rsyslogd -N1 2>&1
echo ""
echo "--- Archivos de log configurados ---"
grep -rohE "/var/log/\S+" /etc/rsyslog.conf /etc/rsyslog.d/*.conf 2>/dev/null | \
    sort -u
echo ""
echo "--- Archivos de log existentes ---"
ls /var/log/*.log /var/log/syslog /var/log/messages 2>/dev/null
'
```

### Paso 3.4: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "--- Servicio ---"
systemctl is-active rsyslog 2>/dev/null || echo "rsyslog: $(service rsyslog status 2>/dev/null | head -1)"
echo ""
echo "--- Input ---"
grep -rhE "imuxsock|imjournal|imklog" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#" | head -5
echo ""
echo "--- Log principal ---"
ls -la /var/log/syslog 2>/dev/null && echo "  Debian usa /var/log/syslog"
echo ""
echo "--- Propietario de logs ---"
ls -la /var/log/syslog 2>/dev/null | awk "{print \"  Propietario: \" \$3 \":\" \$4}"
'
echo ""
docker compose exec alma-dev bash -c '
echo "=== RHEL ==="
echo ""
echo "--- Servicio ---"
systemctl is-active rsyslog 2>/dev/null || echo "rsyslog: no activo"
echo ""
echo "--- Input ---"
grep -rhE "imuxsock|imjournal|imklog" /etc/rsyslog.conf /etc/rsyslog.d/ 2>/dev/null | grep -v "^#" | head -5
echo ""
echo "--- Log principal ---"
ls -la /var/log/messages 2>/dev/null && echo "  RHEL usa /var/log/messages"
echo ""
echo "--- Propietario de logs ---"
ls -la /var/log/messages 2>/dev/null | awk "{print \"  Propietario: \" \$3 \":\" \$4}"
'
```

### Paso 3.5: Tabla comparativa Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparativa Debian vs RHEL ==="
echo ""
echo "| Aspecto              | Debian/Ubuntu            | RHEL/Fedora         |"
echo "|----------------------|--------------------------|---------------------|"
echo "| Reglas por defecto   | /etc/rsyslog.d/50-default| En rsyslog.conf     |"
echo "| Input de journal     | imuxsock (socket)        | imjournal (directo) |"
echo "| Log principal        | /var/log/syslog          | /var/log/messages   |"
echo "| Log de auth          | /var/log/auth.log        | /var/log/secure     |"
echo "| Log de correo        | /var/log/mail.log        | /var/log/maillog    |"
echo "| Log de cron          | En syslog                | /var/log/cron       |"
echo "| Propietario          | syslog:adm               | root:root           |"
echo "| Permisos             | 0640                     | 0600                |"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Cada mensaje syslog tiene tres componentes: facility (quien),
   priority (gravedad) y action (destino). rsyslog enruta mensajes
   segun reglas facility.priority.

2. Los operadores de prioridad: .priority (esta y superiores),
   .=priority (solo esta), .none (excluir). Una regla puede
   combinar facilidades con coma y selectores con punto y coma.

3. /etc/rsyslog.d/ permite configuracion modular. Los archivos
   se procesan en orden alfabetico. Convencion: 00-19 modulos,
   20-49 apps, 50-79 defaults, 80-99 catch-all.

4. El guion (-) antes de un archivo desactiva fsync para mejor
   rendimiento. Usar sin guion para logs criticos.

5. logger envia mensajes a syslog con -p facility.priority y
   -t tag. Es la herramienta principal para logging desde scripts.

6. Debian usa /var/log/syslog y imuxsock. RHEL usa
   /var/log/messages e imjournal. Los archivos de auth se
   llaman auth.log en Debian y secure en RHEL.
