# T02 — Campos personalizados

> **Fuentes:** `README.md`, `README.max.md`, `LABS.md`, `labs/README.md`
> **Regla aplicada:** 2 (`.max.md` sin `.claude.md` → crear `.claude.md`)

---

## Erratas detectadas

| # | Archivo | Línea(s) | Error | Corrección |
|---|---------|----------|-------|------------|
| 1 | README.max.md | 34 | "Campos del sistema (_前缀)" — Caracteres chinos "前缀" | Debe ser **"(prefijo _)"** en español |
| 2 | README.max.md | 76 | "Campos internos (__前缀)" — Caracteres chinos | Debe ser **"(prefijo __)"** |
| 3 | README.md | 147 | `a]4cc150f2a94f4a84b2b8fd40e2a913c` — System boot | Typo: `]` por `4`. Debe ser **`a4cc150f2a94f4a84b2b8fd40e2a913c`**. `README.max.md:167` lo tiene correctamente |
| 4 | README.max.md | 476 | "Usuario iniciou sesión" | **"inició"** — "iniciou" es portugués |
| 5 | README.max.md | 481–483 | Comentarios sugieren `journalctl ... ACTION=login` y `USER=admin` como campos filtrables | `systemd-cat` no crea campos personalizados en el journal; `USER` y `ACTION` quedan embebidos en el texto de MESSAGE. Para filtrar: `--grep="ACTION=login"`. Para campos reales: usar `sd_journal_send()` o `LogExtraFields` |
| 6 | README.md + README.max.md | 156 / 178 | `journalctl --catalog --message-id=UUID` | `--message-id` no es un flag válido. La forma correcta es **`journalctl --list-catalog UUID`** |

---

## Estructura de campos del journal

Cada entrada del journal es un conjunto de **campos clave-valor**. Se dividen en tres categorías según su prefijo:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    CATEGORÍAS DE CAMPOS                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  PREFIJO _  — Establecidos por journald/kernel                             │
│  ═══════════════════════════════════════════                               │
│  Confiables para auditoría — la aplicación NO puede falsificarlos          │
│  _PID, _UID, _COMM, _EXE, _HOSTNAME, _SYSTEMD_UNIT, _BOOT_ID...          │
│                                                                             │
│  SIN PREFIJO — Establecidos por la aplicación                              │
│  ════════════════════════════════════════════                               │
│  MESSAGE, PRIORITY, SYSLOG_IDENTIFIER, CODE_FILE, CODE_LINE...            │
│  La aplicación tiene control total sobre estos valores                     │
│                                                                             │
│  PREFIJO __ — Uso interno de journald                                      │
│  ════════════════════════════════════════                                   │
│  __CURSOR, __REALTIME_TIMESTAMP, __MONOTONIC_TIMESTAMP                     │
│  Solo uso interno (excepto __CURSOR para lecturas incrementales)           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Campos del sistema (prefijo _)

**Confiables para auditoría** — el kernel y journald los establecen, no la aplicación.

| Campo | Descripción | Ejemplo |
|-------|-------------|---------|
| `_PID` | PID del proceso | `1234` |
| `_UID` | UID del usuario | `0` |
| `_GID` | GID del grupo | `0` |
| `_COMM` | Nombre del comando (basename) | `sshd` |
| `_EXE` | Ruta completa del ejecutable | `/usr/sbin/sshd` |
| `_CMDLINE` | Línea de comandos completa | `sshd: user [priv]` |
| `_HOSTNAME` | Hostname | `server01` |
| `_TRANSPORT` | Cómo llegó el log | `syslog`, `stdout`, `journal` |
| `_SYSTEMD_UNIT` | Unidad de systemd | `sshd.service` |
| `_SYSTEMD_CGROUP` | Cgroup | `/system.slice/sshd.service` |
| `_BOOT_ID` | ID único del boot | `abc123...` |
| `_MACHINE_ID` | ID de la máquina | `def456...` |
| `_SOURCE_REALTIME_TIMESTAMP` | Timestamp original del mensaje | `1710689400123456` |

```bash
# Ver todos los campos de un mensaje:
journalctl -n 1 -o verbose
# Campos con _ los establece journald — la app no puede modificarlos
```

## Campos de usuario (sin prefijo)

**Controlados por la aplicación** — pueden ser modificados o falsificados.

| Campo | Descripción | Ejemplo |
|-------|-------------|---------|
| `MESSAGE` | Texto del log | `User login successful` |
| `PRIORITY` | Nivel 0-7 | `6` (info) |
| `SYSLOG_FACILITY` | Facility syslog (numérico) | `4` (auth) |
| `SYSLOG_IDENTIFIER` | Tag/identificador | `sshd` |
| `SYSLOG_PID` | PID reportado por la app | `1234` |
| `CODE_FILE` | Archivo fuente | `main.c` |
| `CODE_LINE` | Línea del código | `142` |
| `CODE_FUNC` | Función | `handle_request` |

## Campos internos (prefijo __)

| Campo | Descripción | Uso |
|-------|-------------|-----|
| `__CURSOR` | Posición única en el journal | `--after-cursor` para lecturas incrementales |
| `__REALTIME_TIMESTAMP` | Timestamp en microsegundos | Representación canónica del tiempo |
| `__MONOTONIC_TIMESTAMP` | Timestamp monotónico | Tiempo desde el boot (para correlacionar) |

---

## SYSLOG_IDENTIFIER

El campo más usado para identificar la fuente de un mensaje:

```bash
# Es el "tag" del formato short:
# Mar 17 15:30:00 server sshd[1234]: mensaje
#                        ^^^^
#                        SYSLOG_IDENTIFIER

# Filtrar por identifier:
journalctl SYSLOG_IDENTIFIER=sshd
journalctl SYSLOG_IDENTIFIER=sudo

# Listar todos los identifiers:
journalctl -F SYSLOG_IDENTIFIER | head -20
```

### SYSLOG_IDENTIFIER vs _COMM

```
SYSLOG_IDENTIFIER = lo que la app dice ser (configurable)
_COMM             = lo que realmente es el ejecutable (confiable)

Pueden diferir:
  _COMM=apache2    SYSLOG_IDENTIFIER=apache2     → coinciden
  _COMM=python3    SYSLOG_IDENTIFIER=myapp        → difieren (app Python)
  _COMM=bash       SYSLOG_IDENTIFIER=backup.sh    → difieren (script)

Para filtrar por identidad declarada:     SYSLOG_IDENTIFIER
Para filtrar por ejecutable real:          _COMM o _EXE
```

> **Auditoría:** Una app maliciosa puede poner `SYSLOG_IDENTIFIER=sshd` para disfrazar sus logs. Pero `_COMM` y `_EXE` revelan el ejecutable real. Para auditoría, confiar en los campos con `_`.

### Establecer SYSLOG_IDENTIFIER

```bash
# Con logger:
logger -t myapp "mensaje de mi aplicación"
# SYSLOG_IDENTIFIER=myapp

# Con systemd-cat (más integrado con journald):
echo "mensaje" | systemd-cat -t myapp -p info
# SYSLOG_IDENTIFIER=myapp

# En un unit file de systemd:
# [Service]
# SyslogIdentifier=myapp
# Todos los logs de stdout/stderr tendrán este identifier
```

---

## MESSAGE_ID

UUID que identifica un **tipo** de mensaje — permite buscar una clase de evento sin depender del texto:

```bash
# MESSAGE_IDs comunes de systemd:
journalctl MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -n 5
# Todos los "Unit started" en el journal

journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 -n 5
# Todos los "Unit failed"

# Ver el MESSAGE_ID de una entrada:
journalctl -u nginx -o verbose -n 1 | grep MESSAGE_ID
```

### MESSAGE_IDs comunes de systemd

```
┌────────────────────────────────────────┬──────────────────────────────────┐
│ MESSAGE_ID                             │ Evento                           │
├────────────────────────────────────────┼──────────────────────────────────┤
│ 39f53479d3a045ac8e11786248231fbf       │ Unit started (arrancada)         │
│ be02cf6855d2428ba40df7e9d022f03d       │ Unit stopped (detenida)          │
│ 7d4958e842da4a758f6c1cdc7b36dcc5       │ Unit failed (fallida)            │
│ 98268866d1d54a499c4e98921d93bc40       │ Unit reloaded                    │
│ a4cc150f2a94f4a84b2b8fd40e2a913c       │ System booted                    │
│ d34d037fff1847e6ae669a370e694725       │ Seat started (sesión gráfica)    │
│ 8d45620c1a4348dbb17410da57c60c66       │ New user session                 │
└────────────────────────────────────────┴──────────────────────────────────┘
```

```bash
# Catálogo completo de systemd:
journalctl --list-catalog | head -50

# Buscar la explicación de un MESSAGE_ID específico:
journalctl --list-catalog 7d4958e842da4a758f6c1cdc7b36dcc5
```

### Monitoreo con MESSAGE_ID

```bash
# Contar servicios que fallaron hoy:
journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 \
    --since today -o json --no-pager | \
    jq -r '.UNIT' | sort -u

# Alertar en tiempo real cuando un servicio falle:
journalctl -f -o json MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 | \
    while read -r line; do
        UNIT=$(echo "$line" | jq -r '.UNIT')
        echo "ALERTA: $UNIT ha fallado a las $(date)"
    done
```

---

## _TRANSPORT — Cómo llegó el log

```bash
# _TRANSPORT indica el origen del mensaje:
journalctl -F _TRANSPORT
# journal    — API sd_journal_send() directa
# stdout     — stdout/stderr de un servicio systemd
# syslog     — a través de syslog() o /dev/log
# kernel     — mensajes del kernel (dmesg)
# audit      — subsistema de auditoría del kernel
# driver     — mensajes internos de journald

# Filtrar por transport:
journalctl _TRANSPORT=stdout -u nginx       # solo stdout/stderr de nginx
journalctl _TRANSPORT=kernel -p err         # solo errores del kernel
journalctl _TRANSPORT=audit                 # solo mensajes de auditoría
```

---

## Campos personalizados desde aplicaciones

### systemd-cat

```bash
# Conectar la salida de un comando al journal:
echo "mensaje personalizado" | systemd-cat -t myapp -p info
# Crea:
#   SYSLOG_IDENTIFIER=myapp
#   PRIORITY=6
#   MESSAGE=mensaje personalizado

# Ejecutar un script con toda su salida al journal:
systemd-cat -t backup /opt/backup/run.sh
# Todo stdout/stderr va al journal con tag "backup"
```

> **Limitación de systemd-cat:** Solo establece `SYSLOG_IDENTIFIER` (-t) y `PRIORITY` (-p). No puede crear campos personalizados arbitrarios. Para eso, usar `sd_journal_send()` o `LogExtraFields`.

### sd_journal_send() — C

```c
#include <systemd/sd-journal.h>

sd_journal_send("MESSAGE=User login",
                "PRIORITY=6",
                "USER_NAME=%s", username,
                "LOGIN_IP=%s", ip,
                "LOGIN_METHOD=ssh",
                NULL);

// Filtrar después:
// journalctl USER_NAME=admin
// journalctl LOGIN_METHOD=ssh
```

### Python (systemd.journal)

```python
from systemd import journal

journal.send("User login",
             PRIORITY=journal.LOG_INFO,
             USER_NAME=username,
             LOGIN_IP=ip,
             LOGIN_METHOD="ssh")

# journalctl USER_NAME=admin
```

> **Diferencia clave:** `sd_journal_send()` y `journal.send()` crean campos **reales** en el journal, filtrables con `journalctl CAMPO=valor`. `systemd-cat` y `logger` solo establecen `SYSLOG_IDENTIFIER` y `PRIORITY`; cualquier dato extra queda como texto dentro de `MESSAGE`.

### logger con datos estructurados

```bash
# logger soporta RFC 5424 con datos estructurados:
logger --sd-id=myapp@12345 --sd-param=user=admin --sd-param=action=login \
    "Usuario admin ha iniciado sesión"

# Sin embargo, journald NO parsea estos campos como campos separados
# del journal. Quedan como parte del mensaje.
# Para campos personalizados reales, usar sd_journal_send() o LogExtraFields
```

### En unit files — LogExtraFields

```bash
# [Service]
# SyslogIdentifier=myapp
# LogExtraFields=APP_VERSION=2.1.0
# LogExtraFields=ENVIRONMENT=production
# LogExtraFields=TEAM=backend

# Resultado: TODOS los logs de este servicio incluyen automáticamente:
#   APP_VERSION=2.1.0
#   ENVIRONMENT=production
#   TEAM=backend

# Filtrar por campo personalizado:
journalctl ENVIRONMENT=production              # todos los servicios de producción
journalctl TEAM=backend -p err                 # errores del equipo backend
journalctl APP_VERSION=2.1.0 -u myapp          # logs de una versión específica
```

> `LogExtraFields` crea campos **reales** en el journal — es la forma más simple de agregar metadata filtrable sin modificar la aplicación.

---

## Campos de código fuente

```bash
# Algunas aplicaciones incluyen información de debug:
journalctl -u systemd-logind -o verbose -n 1 | grep CODE_
# CODE_FILE=src/login/logind.c
# CODE_LINE=542
# CODE_FUNC=manager_handle_action

# Filtrar por función (útil para debug):
journalctl CODE_FUNC=handle_request -n 5 --no-pager

# Filtrar por archivo fuente:
journalctl CODE_FILE=src/login/logind.c -n 10 --no-pager
```

---

## SyslogIdentifier y directivas de unit files

```bash
# En un archivo .service:

# [Service]
# SyslogIdentifier=myapp
# → SYSLOG_IDENTIFIER=myapp (en lugar del nombre del ejecutable)

# SyslogFacility=local0
# → SYSLOG_FACILITY=16 (local0)

# SyslogLevelPrefix=true (default)
# → Si la app imprime "<3>error message", journald interpreta
#   <3> como prioridad err (protocolo de prioridad del kernel)

# LogExtraFields=APP_VERSION=2.1.0
# LogExtraFields=ENVIRONMENT=production
# → Campos personalizados en TODAS las entradas del servicio
```

---

## Exploración de campos

```bash
# Ver TODOS los campos de un mensaje:
journalctl -u sshd -n 1 -o json-pretty | jq 'keys[]' | sort

# Listar valores únicos de un campo:
journalctl -F SYSLOG_IDENTIFIER | head -20    # identifiers
journalctl -F _TRANSPORT                      # transports
journalctl -F _UID --since today              # UIDs activos hoy
journalctl -F PRIORITY | sort                 # prioridades usadas

# Top 10 ejecutables por volumen de logs:
journalctl -b -o json --no-pager | \
    jq -r '._COMM // "unknown"' | sort | uniq -c | sort -rn | head -10
```

---

## Quick reference

```
CAMPOS DEL SISTEMA (_):
  _PID, _UID, _GID       — identificación del proceso
  _COMM, _EXE, _CMDLINE  — ejecutable
  _HOSTNAME               — hostname
  _SYSTEMD_UNIT            — unidad de systemd
  _TRANSPORT               — journal|stdout|syslog|kernel|audit
  _BOOT_ID, _MACHINE_ID   — identificadores del sistema

CAMPOS DE USUARIO:
  MESSAGE                  — texto del log
  PRIORITY (0-7)           — severidad
  SYSLOG_IDENTIFIER        — tag de la aplicación (configurable)
  CODE_FILE/LINE/FUNC      — código fuente

CAMPOS INTERNOS (__):
  __CURSOR                 — posición para lectura incremental
  __REALTIME_TIMESTAMP     — timestamp canónico (μs)
  __MONOTONIC_TIMESTAMP    — tiempo desde boot (μs)

FILTRAR:
  journalctl CAMPO=valor
  journalctl -F CAMPO         listar valores únicos

ESTABLECER SYSLOG_IDENTIFIER:
  logger -t myapp "msg"
  echo "msg" | systemd-cat -t myapp
  SyslogIdentifier=myapp       en unit file

CAMPOS PERSONALIZADOS REALES (filtrables):
  sd_journal_send("CUSTOM=value", ...)    desde C
  journal.send("msg", CUSTOM="value")     desde Python
  LogExtraFields=CUSTOM=value             en unit file

MESSAGE_IDs:
  39f53479... → Unit started
  be02cf68... → Unit stopped
  7d4958e8... → Unit failed
  journalctl --list-catalog UUID
```

---

## Labs

### Parte 1 — Tipos de campos

**Paso 1.1: Campos del sistema (prefijo _)**

Los establece journald automáticamente. No pueden ser falsificados por la aplicación. Son confiables para auditoría.

```bash
# Ver campos _ en una entrada real:
journalctl -n 1 -o verbose | grep "^    _"
# _PID=1234
# _UID=0
# _COMM=sshd
# _EXE=/usr/sbin/sshd
# _SYSTEMD_UNIT=sshd.service
# _TRANSPORT=syslog
# ...
```

**Paso 1.2: Campos de usuario (sin prefijo)**

Los establece la aplicación. Son configurables y potencialmente falsificables:

```bash
# Ver campos sin _ en una entrada:
journalctl -n 1 -o verbose | grep -E "^    [A-Z]" | grep -v "^    _"
# MESSAGE=...
# PRIORITY=6
# SYSLOG_IDENTIFIER=sshd
# SYSLOG_PID=1234
```

**Paso 1.3: Campos internos (prefijo __)**

Control interno de journald. Solo `__CURSOR` es útil (para retomar lecturas incrementales):

```bash
journalctl -n 1 -o verbose | grep "^    __"
# __CURSOR=s=abc;i=456;...
# __REALTIME_TIMESTAMP=1710689400123456
# __MONOTONIC_TIMESTAMP=12345678
```

**Paso 1.4: _TRANSPORT**

Indica cómo llegó el mensaje a journald:

| Transport | Origen |
|-----------|--------|
| `journal` | API directa `sd_journal_send()` |
| `stdout` | stdout/stderr de servicio systemd |
| `syslog` | API `syslog()` o `/dev/log` |
| `kernel` | Mensajes del kernel (dmesg) |
| `audit` | Subsistema de auditoría |
| `driver` | Mensajes internos de journald |

```bash
journalctl -F _TRANSPORT               # listar transports en el sistema
journalctl _TRANSPORT=kernel -p err     # solo errores del kernel
journalctl _TRANSPORT=stdout -u nginx   # solo stdout de nginx
```

### Parte 2 — Identifiers y MESSAGE_ID

**Paso 2.1: SYSLOG_IDENTIFIER**

El tag del mensaje syslog — lo que aparece entre hostname y PID en el formato short:

```bash
journalctl SYSLOG_IDENTIFIER=sshd      # filtrar por identifier
journalctl -F SYSLOG_IDENTIFIER        # listar todos los identifiers
```

**Paso 2.2: SYSLOG_IDENTIFIER vs _COMM**

`SYSLOG_IDENTIFIER` es lo que la app dice ser (configurable). `_COMM` es el ejecutable real (lo pone journald, confiable). Pueden diferir:

```bash
# Probar:
logger -t lab-ident "Mensaje de prueba"
sleep 1
journalctl SYSLOG_IDENTIFIER=lab-ident -n 1 -o verbose | \
    grep -E "SYSLOG_IDENTIFIER|_COMM|MESSAGE"
# SYSLOG_IDENTIFIER=lab-ident
# _COMM=logger                ← el ejecutable real es "logger", no "lab-ident"
```

**Paso 2.3: MESSAGE_ID**

UUID que identifica un tipo de evento. Permite buscar "servicios que fallaron" sin depender del texto:

```bash
# Servicios arrancados en este boot:
journalctl MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -b -n 5

# Servicios fallidos:
journalctl MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 -b

# Catálogo completo:
journalctl --list-catalog | head -50

# Explicación de un MESSAGE_ID:
journalctl --list-catalog 7d4958e842da4a758f6c1cdc7b36dcc5
```

### Parte 3 — Campos custom y exploración

**Paso 3.1: systemd-cat**

```bash
# Enviar mensaje con identifier personalizado:
echo "mensaje personalizado desde lab" | systemd-cat -t lab-custom -p info

# Verificar:
journalctl SYSLOG_IDENTIFIER=lab-custom -n 1 -o verbose
```

`systemd-cat` solo controla SYSLOG_IDENTIFIER y PRIORITY. No puede crear campos arbitrarios.

**Paso 3.2: SyslogIdentifier y LogExtraFields en units**

```bash
# En un .service:
# [Service]
# SyslogIdentifier=myapp
# LogExtraFields=APP_VERSION=2.1.0
# LogExtraFields=ENVIRONMENT=production
# LogExtraFields=TEAM=backend

# LogExtraFields SÍ crea campos reales filtrables:
journalctl ENVIRONMENT=production
journalctl TEAM=backend -p err
```

**Paso 3.3: Exploración de campos**

```bash
# Todos los campos de una entrada:
journalctl -n 1 -o json-pretty | jq 'keys[]' | sort

# Valores únicos de un campo:
journalctl -F PRIORITY -b | sort

# UIDs que generan logs:
journalctl -F _UID -b | sort -n

# Top 10 ejecutables por volumen:
journalctl -b -o json --no-pager | \
    jq -r '._COMM // "unknown"' | sort | uniq -c | sort -rn | head -10
```

**Paso 3.4: Campos de código fuente**

```bash
# Buscar entradas con CODE_FILE:
journalctl -b -o verbose | grep "CODE_FILE" | head -5

# systemd incluye estos campos en sus propios mensajes:
journalctl -o verbose -n 1 | grep "CODE_"
# CODE_FILE=src/login/logind.c
# CODE_LINE=542
# CODE_FUNC=manager_handle_action
```

---

## Ejercicios

### Ejercicio 1 — Categorías de campos

Para cada campo, indica su categoría (sistema, usuario, interno) y quién lo establece:

- `_PID`
- `MESSAGE`
- `__CURSOR`
- `SYSLOG_IDENTIFIER`
- `_TRANSPORT`
- `CODE_FILE`
- `_COMM`
- `PRIORITY`

<details><summary>Predicción</summary>

| Campo | Categoría | Quién lo establece |
|-------|-----------|--------------------|
| `_PID` | Sistema (prefijo `_`) | journald/kernel — confiable |
| `MESSAGE` | Usuario (sin prefijo) | La aplicación |
| `__CURSOR` | Interno (prefijo `__`) | journald internamente |
| `SYSLOG_IDENTIFIER` | Usuario (sin prefijo) | La aplicación (configurable) |
| `_TRANSPORT` | Sistema (prefijo `_`) | journald — indica cómo llegó |
| `CODE_FILE` | Usuario (sin prefijo) | La aplicación (opcional) |
| `_COMM` | Sistema (prefijo `_`) | journald — nombre real del ejecutable |
| `PRIORITY` | Usuario (sin prefijo) | La aplicación (0-7) |

Regla mnemónica: con `_` = confiable (sistema). Sin `_` = la app lo controla. Con `__` = interno.

</details>

### Ejercicio 2 — SYSLOG_IDENTIFIER vs _COMM

Un script Python llamado `/opt/myapp/app.py` se ejecuta como servicio de systemd con `SyslogIdentifier=myapp`. ¿Qué valores tendrán estos campos?

<details><summary>Predicción</summary>

- `SYSLOG_IDENTIFIER` = `myapp` — lo establece la directiva `SyslogIdentifier` del unit file
- `_COMM` = `python3` — journald usa el basename del ejecutable real
- `_EXE` = `/usr/bin/python3` — la ruta completa del intérprete
- `_CMDLINE` = `/usr/bin/python3 /opt/myapp/app.py` — la línea de comandos completa

Para filtrar este servicio:
- `journalctl SYSLOG_IDENTIFIER=myapp` — por identidad declarada
- `journalctl _COMM=python3` — captura TODAS las apps Python, no solo myapp
- `journalctl -u myapp.service` — la forma más precisa (por unidad systemd)

</details>

### Ejercicio 3 — MESSAGE_ID para monitoreo

Escribe un pipeline que muestre cuántos servicios se arrancaron, cuántos se detuvieron y cuántos fallaron en el boot actual.

<details><summary>Predicción</summary>

```bash
echo "Arrancados:"
journalctl -b MESSAGE_ID=39f53479d3a045ac8e11786248231fbf -o json --no-pager | \
    jq -r '.UNIT // .USER_UNIT // "unknown"' | sort -u | wc -l

echo "Detenidos:"
journalctl -b MESSAGE_ID=be02cf6855d2428ba40df7e9d022f03d -o json --no-pager | \
    jq -r '.UNIT // .USER_UNIT // "unknown"' | sort -u | wc -l

echo "Fallidos:"
journalctl -b MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5 -o json --no-pager | \
    jq -r '.UNIT // .USER_UNIT // "unknown"' | sort -u | wc -l
```

`sort -u` elimina duplicados — un servicio que se reinició varias veces se cuenta una sola vez. `//` en jq es fallback: primero intenta UNIT, luego USER_UNIT, luego "unknown".

</details>

### Ejercicio 4 — Campos personalizados reales vs texto

¿Cuál es la diferencia funcional entre estas tres formas de enviar datos personalizados?

```bash
# Forma A:
logger -t myapp "user=admin action=login"

# Forma B:
echo "user login" | systemd-cat -t myapp -p info

# Forma C (en unit file):
# LogExtraFields=USER_ACTION=login
# LogExtraFields=APP_USER=admin
```

<details><summary>Predicción</summary>

**Forma A (logger):** `USER_ACTION` y `APP_USER` no son campos del journal — son texto dentro de `MESSAGE`. Para buscar: `journalctl --grep="user=admin"` (búsqueda en texto, lenta). `journalctl USER_ACTION=login` **no funciona**.

**Forma B (systemd-cat):** Igual que A — solo controla `SYSLOG_IDENTIFIER` y `PRIORITY`. "user login" es texto en `MESSAGE`. No crea campos filtrables.

**Forma C (LogExtraFields):** Crea campos **reales** en el journal. `journalctl USER_ACTION=login` **sí funciona** (filtrado indexado, rápido). `journalctl APP_USER=admin` también funciona. Cada entrada del servicio incluye automáticamente estos campos.

Resumen:
- A y B: datos en el texto → `--grep` (O(n), lento)
- C: campos reales → `journalctl CAMPO=valor` (O(1), indexado)
- Para campos reales sin unit file: usar `sd_journal_send()` en C/Python

</details>

### Ejercicio 5 — _TRANSPORT

¿Qué `_TRANSPORT` tendría cada uno de estos mensajes?

1. `nginx` escribe en stdout y systemd captura la salida
2. Un script usa `logger -t backup "done"`
3. El kernel detecta un error de disco
4. Una app Python usa `journal.send("msg")`
5. `auditd` registra un cambio de archivo

<details><summary>Predicción</summary>

1. **`stdout`** — nginx escribe en stdout/stderr, systemd lo redirige al journal
2. **`syslog`** — `logger` usa la API syslog() vía `/dev/log`
3. **`kernel`** — mensajes del kernel van por el buffer de kernel (como dmesg)
4. **`journal`** — `journal.send()` usa la API directa `sd_journal_send()`
5. **`audit`** — el subsistema de auditoría del kernel tiene su propio transport

Para filtrar solo mensajes de la API nativa: `journalctl _TRANSPORT=journal`
Para filtrar solo stdout de servicios: `journalctl _TRANSPORT=stdout`

</details>

### Ejercicio 6 — Exploración de campos

Un servicio `myapp` está generando errores. ¿Cómo descubres qué campos están disponibles en sus mensajes para crear filtros más específicos?

<details><summary>Predicción</summary>

```bash
# 1. Ver TODOS los campos de una entrada:
journalctl -u myapp -n 1 -o verbose

# 2. Listar solo los nombres de campos (con jq):
journalctl -u myapp -n 1 -o json-pretty | jq 'keys[]' | sort

# 3. Si tiene LogExtraFields, ver qué campos custom tiene:
journalctl -u myapp -n 1 -o json-pretty | jq 'to_entries[] | select(.key | test("^[A-Z]") and (startswith("_") | not) and (. != "MESSAGE") and (. != "PRIORITY") and (. != "SYSLOG")) | .key'

# 4. Ver valores únicos de un campo encontrado:
journalctl -u myapp -F APP_VERSION --since today
```

El flujo es: `verbose` o `json-pretty` para descubrir campos → `-F CAMPO` para ver valores posibles → `journalctl CAMPO=valor` para filtrar.

</details>

### Ejercicio 7 — LogExtraFields en producción

Diseña los `LogExtraFields` para un servicio `api-server` que quieres poder filtrar por:
- Entorno (production/staging/development)
- Equipo responsable (backend)
- Versión de la API (2.3.1)

<details><summary>Predicción</summary>

En el unit file `/etc/systemd/system/api-server.service`:

```ini
[Service]
SyslogIdentifier=api-server
LogExtraFields=ENVIRONMENT=production
LogExtraFields=TEAM=backend
LogExtraFields=API_VERSION=2.3.1
```

Después de `systemctl daemon-reload && systemctl restart api-server`:

```bash
# Errores de producción:
journalctl ENVIRONMENT=production -p err --since today

# Errores de todo el equipo backend:
journalctl TEAM=backend -p err --since today

# Logs de una versión específica (útil en rollbacks):
journalctl API_VERSION=2.3.1 -u api-server --since "1 hour ago"

# Combinar campos (AND):
journalctl ENVIRONMENT=production TEAM=backend -p warning
```

Ventaja: si tienes 10 servicios del equipo backend, todos con `LogExtraFields=TEAM=backend`, una sola consulta muestra los errores de todos ellos.

</details>

### Ejercicio 8 — Confiabilidad de campos

Un incidente de seguridad muestra estas dos entradas del journal:

```
Entrada 1:
  SYSLOG_IDENTIFIER=sshd
  _COMM=sshd
  _EXE=/usr/sbin/sshd
  MESSAGE=Accepted publickey for admin

Entrada 2:
  SYSLOG_IDENTIFIER=sshd
  _COMM=nc
  _EXE=/usr/bin/nc
  MESSAGE=Accepted publickey for admin
```

¿Cuál es sospechosa y por qué?

<details><summary>Predicción</summary>

**Entrada 2 es sospechosa.**

- `SYSLOG_IDENTIFIER=sshd` dice ser sshd, pero `_COMM=nc` y `_EXE=/usr/bin/nc` revelan que el ejecutable real es `nc` (netcat)
- Alguien está usando netcat para inyectar mensajes falsos en el log que imitan mensajes de sshd
- Podría ser un atacante intentando:
  - Disfrazar actividad maliciosa como logins legítimos
  - Cubrir sus huellas con logs falsos

**Los campos `_COMM` y `_EXE` son confiables** (los establece journald/kernel). `SYSLOG_IDENTIFIER` lo controla la aplicación y puede ser falsificado.

Para auditoría: siempre verificar que `SYSLOG_IDENTIFIER` coincida con `_COMM`/`_EXE`. Detectar discrepancias:

```bash
journalctl -b -o json --no-pager | \
    jq -r 'select(.SYSLOG_IDENTIFIER != ._COMM) | "\(.SYSLOG_IDENTIFIER) vs \(._COMM)"' | \
    sort | uniq -c | sort -rn
```

</details>

### Ejercicio 9 — Catálogo de mensajes

¿Cómo usarías `journalctl -x` vs `journalctl --list-catalog` y cuándo preferirías cada uno?

<details><summary>Predicción</summary>

**`journalctl -x` (o `--catalog`):** Añade texto explicativo del catálogo a la salida normal de logs. Muestra los logs con explicaciones in-line:

```bash
journalctl -x -u nginx --since "5 min ago"
# Mar 25 10:30:00 server systemd[1]: Started nginx.service
# -- Subject: A start job for unit nginx.service has finished successfully
# -- Defined-By: systemd
# -- The start job for unit nginx.service has finished successfully.
```

**`journalctl --list-catalog [UUID]`:** Muestra las entradas del catálogo sin logs. Solo las descripciones:

```bash
journalctl --list-catalog 7d4958e842da4a758f6c1cdc7b36dcc5
# -- Unit failed
# -- The unit UNIT has entered the 'failed' state with result RESULT.
```

Cuándo usar cada uno:
- `-x` → investigando un incidente, quieres entender QUÉ SIGNIFICAN los mensajes que ves
- `--list-catalog` → buscando la explicación de un MESSAGE_ID específico, sin ver logs
- `-x` es para consumo humano durante debug
- `--list-catalog` es para referencia rápida sobre tipos de eventos

</details>

### Ejercicio 10 — Diseño de logging estructurado

Tienes una aplicación Python que procesa pedidos. Diseña los campos personalizados que enviarías al journal para poder:
1. Filtrar por tipo de evento (order_created, order_shipped, payment_failed)
2. Buscar todos los eventos de un pedido específico
3. Ver errores de pago de un cliente específico

<details><summary>Predicción</summary>

```python
from systemd import journal

# Crear pedido:
journal.send("Order created",
             PRIORITY=journal.LOG_INFO,
             EVENT_TYPE="order_created",
             ORDER_ID="ORD-2026-001",
             CUSTOMER_ID="CUST-42",
             ORDER_TOTAL="149.99")

# Enviar pedido:
journal.send("Order shipped",
             PRIORITY=journal.LOG_INFO,
             EVENT_TYPE="order_shipped",
             ORDER_ID="ORD-2026-001",
             TRACKING="TRACK-ABC123")

# Error de pago:
journal.send("Payment failed: card declined",
             PRIORITY=journal.LOG_ERR,
             EVENT_TYPE="payment_failed",
             ORDER_ID="ORD-2026-001",
             CUSTOMER_ID="CUST-42",
             ERROR_CODE="card_declined")
```

Consultas:

```bash
# 1. Filtrar por tipo de evento:
journalctl EVENT_TYPE=order_created --since today
journalctl EVENT_TYPE=payment_failed --since today

# 2. Todos los eventos de un pedido:
journalctl ORDER_ID=ORD-2026-001

# 3. Errores de pago de un cliente:
journalctl EVENT_TYPE=payment_failed CUSTOMER_ID=CUST-42

# Bonus — contar errores de pago por tipo:
journalctl EVENT_TYPE=payment_failed --since today -o json --no-pager | \
    jq -r '.ERROR_CODE' | sort | uniq -c | sort -rn
```

Campos reales del journal (no texto en MESSAGE) permiten filtrado O(1) por los índices de journald, en lugar de `--grep` O(n).

</details>
