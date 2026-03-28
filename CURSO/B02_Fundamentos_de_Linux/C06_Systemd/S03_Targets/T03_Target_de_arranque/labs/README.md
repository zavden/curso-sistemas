# Lab — Target de arranque

## Objetivo

Entender como systemd decide que arrancar al boot: la cadena de
decision, el proceso de boot en fases, el analisis con
systemd-analyze (blame, critical-chain, verify, plot),
identificacion de cuellos de botella, parametros del kernel para
systemd, y debug-shell.service.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Cadena de decision

### Objetivo

Entender como systemd decide que target arrancar y las fases
del proceso de boot.

### Paso 1.1: Como decide systemd

```bash
docker compose exec debian-dev bash -c '
echo "=== Cadena de decision ==="
echo ""
echo "1. Hay systemd.unit= en el kernel command line?"
echo "   Si → usar ese target"
echo "   No ↓"
echo ""
echo "2. Existe /etc/systemd/system/default.target?"
echo "   Si → usar ese symlink"
echo "   No ↓"
echo ""
echo "3. Existe /usr/lib/systemd/system/default.target?"
echo "   Si → usar ese symlink"
echo "   No → arrancar en emergency mode"
echo ""
echo "--- Verificar ---"
echo "Kernel cmdline:"
cat /proc/cmdline 2>/dev/null || echo "  (no disponible en contenedor)"
echo ""
echo "default.target (admin):"
ls -la /etc/systemd/system/default.target 2>/dev/null || \
    echo "  (no existe en /etc/)"
echo ""
echo "default.target (paquete):"
ls -la /usr/lib/systemd/system/default.target 2>/dev/null || \
    ls -la /lib/systemd/system/default.target 2>/dev/null || \
    echo "  (no existe en /usr/lib/)"
'
```

### Paso 1.2: Proceso de boot paso a paso

```bash
docker compose exec debian-dev bash -c '
echo "=== Proceso de boot ==="
echo ""
echo "Fase 1: Hardware → systemd"
echo "  BIOS/UEFI → GRUB → kernel → initramfs → systemd (PID 1)"
echo ""
echo "--- /sbin/init es symlink a systemd ---"
ls -la /sbin/init 2>/dev/null
echo ""
echo "Fase 2: Resolucion de dependencias"
echo "  1. Lee default.target (ej: multi-user.target)"
echo "  2. Resuelve Wants= y Requires= recursivamente"
echo "  3. Ordena segun After= y Before="
echo "  4. Arranca todo en PARALELO respetando dependencias"
echo ""
echo "Fase 3: Arranque paralelo"
echo "                default.target"
echo "                     │"
echo "          ┌──────────┼──────────┐"
echo "          │          │          │"
echo "     local-fs    network    sockets"
echo "      .target     .target   .target"
echo "       │  │         │         │"
echo "    ┌──┘  └──┐   ┌──┘      ┌──┘"
echo "    │        │   │         │"
echo "  home.    var.  NM.     sshd.    ← arrancan en paralelo"
echo "  mount   mount service  socket"
echo ""
echo "Las unidades sin dependencias de orden entre si"
echo "arrancan SIMULTANEAMENTE (boot rapido)"
'
```

### Paso 1.3: Verificar la cadena de dependencias

```bash
docker compose exec debian-dev bash -c '
echo "=== Dependencias del default target ==="
echo ""
DEFAULT=$(readlink /etc/systemd/system/default.target 2>/dev/null || \
          readlink /usr/lib/systemd/system/default.target 2>/dev/null || \
          readlink /lib/systemd/system/default.target 2>/dev/null)
echo "Default target: $DEFAULT"
echo ""
if [[ -n "$DEFAULT" ]]; then
    TARGET_FILE=$(find /usr/lib/systemd/system /lib/systemd/system \
        -name "$(basename "$DEFAULT")" 2>/dev/null | head -1)
    if [[ -n "$TARGET_FILE" ]]; then
        echo "--- Contenido ---"
        cat "$TARGET_FILE"
        echo ""
        echo "--- Dependencias declaradas ---"
        grep -E "^(Wants|Requires|After|Before)=" "$TARGET_FILE" 2>/dev/null || \
            echo "(sin dependencias directas en el archivo)"
    fi
fi
'
```

---

## Parte 2 — systemd-analyze

### Objetivo

Usar systemd-analyze para analizar el boot y verificar unidades.

### Paso 2.1: systemd-analyze blame

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze blame ==="
echo ""
echo "Muestra cuanto tardo cada unidad en arrancar"
echo "Ordenado de mas lento a mas rapido"
echo ""
echo "Ejemplo de salida tipica en un servidor:"
echo "  5.100s cloud-init.service"
echo "  3.200s NetworkManager-wait-online.service"
echo "  2.100s docker.service"
echo "  1.500s apt-daily.service"
echo "  0.800s nginx.service"
echo ""
echo "Son tiempos wall-clock, no CPU"
echo "NetworkManager-wait-online suele ser de los mas lentos"
echo "(espera a que la red este realmente disponible)"
echo ""
echo "--- Intentar en el contenedor ---"
systemd-analyze blame --no-pager 2>&1 | head -10 || \
    echo "(systemd-analyze no disponible — requiere systemd como PID 1)"
'
```

### Paso 2.2: systemd-analyze critical-chain

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze critical-chain ==="
echo ""
echo "Muestra la cadena de dependencias que determina"
echo "el tiempo total del boot (el camino mas largo)"
echo ""
echo "Ejemplo de salida:"
echo "graphical.target @5.0s"
echo "└─display-manager.service @4.2s +800ms"
echo "  └─multi-user.target @4.1s"
echo "    └─nginx.service @3.5s +600ms"
echo "      └─network-online.target @3.4s"
echo "        └─NetworkManager-wait-online.service @1.2s +2.2s"
echo "          └─NetworkManager.service @1.0s +200ms"
echo "            └─basic.target @0.9s"
echo ""
echo "Cada linea: unidad @cuando_arranco +cuanto_tardo"
echo "La indentacion muestra que dependencia la bloqueo"
echo ""
echo "--- Intentar ---"
systemd-analyze critical-chain --no-pager 2>&1 | head -15 || \
    echo "(requiere systemd como PID 1)"
'
```

### Paso 2.3: systemd-analyze verify

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze verify ==="
echo ""
echo "Verifica errores en archivos de unidad sin cargarlos"
echo ""
echo "--- Archivo correcto ---"
cat > /tmp/lab-verify-ok.service << '\''EOF'\''
[Unit]
Description=Good Service

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity

[Install]
WantedBy=multi-user.target
EOF
echo "Verificando archivo correcto:"
systemd-analyze verify /tmp/lab-verify-ok.service 2>&1 || true
echo ""
echo "--- Archivo con errores ---"
cat > /tmp/lab-verify-bad.service << '\''EOF'\''
[Unit]
Description=Bad Service

[Service]
Type=invalid
ExecStart=no-absolute-path
Restart=maybe

[Install]
WantedBy=multi-user.target
EOF
echo "Verificando archivo con errores:"
systemd-analyze verify /tmp/lab-verify-bad.service 2>&1 || true
echo ""
echo "verify detecta: Type invalido, ruta no absoluta, valores erroneos"
rm /tmp/lab-verify-ok.service /tmp/lab-verify-bad.service
'
```

### Paso 2.4: systemd-analyze plot

```bash
docker compose exec debian-dev bash -c '
echo "=== systemd-analyze plot ==="
echo ""
echo "Genera un grafico SVG con lineas de tiempo de cada unidad"
echo ""
echo "  systemd-analyze plot > /tmp/boot.svg"
echo ""
echo "El SVG muestra:"
echo "  Cada unidad es una barra horizontal"
echo "  Rojo: activando"
echo "  Verde: activo"
echo "  Barras que se solapan = arranque paralelo"
echo "  La barra mas a la derecha define el tiempo total"
echo ""
echo "--- Otros comandos ---"
echo "systemd-analyze time"
echo "  Startup finished in 2.5s (kernel) + 5.1s (userspace) = 7.6s"
echo ""
echo "systemd-analyze dot default.target | dot -Tpng -o deps.png"
echo "  Genera diagrama de dependencias (requiere graphviz)"
echo ""
echo "--- Intentar ---"
systemd-analyze time 2>&1 || \
    echo "(requiere systemd como PID 1)"
'
```

---

## Parte 3 — Optimizacion y debug

### Objetivo

Identificar cuellos de botella del boot y usar herramientas
de debug.

### Paso 3.1: Cuellos de botella comunes

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuellos de botella comunes ==="
echo ""
echo "Servicios tipicamente lentos:"
echo ""
echo "  NetworkManager-wait-online.service"
echo "    Espera la red (3-30s)"
echo "    Solucion: deshabilitar si no se necesita network-online.target"
echo ""
echo "  cloud-init.service"
echo "    Innecesario en bare metal"
echo "    Solucion: systemctl disable cloud-init"
echo ""
echo "  apt-daily.service / apt-daily-upgrade.service"
echo "    Actualizaciones automaticas"
echo "    Solucion: deshabilitar timers si se gestionan manualmente"
echo ""
echo "  docker.service"
echo "    Inicializacion de contenedores"
echo "    Solucion: usar docker.socket (activacion por socket)"
echo ""
echo "  plymouth-quit-wait.service"
echo "    Splash screen"
echo "    Solucion: mask si no se necesita"
'
```

### Paso 3.2: Soluciones de optimizacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Optimizacion del boot ==="
echo ""
echo "--- Deshabilitar servicios innecesarios ---"
echo "systemctl disable NetworkManager-wait-online.service"
echo "systemctl disable apt-daily.timer apt-daily-upgrade.timer"
echo "systemctl mask plymouth-quit-wait.service"
echo ""
echo "--- Cambiar de graphical a multi-user ---"
echo "systemctl set-default multi-user.target"
echo "  Elimina X11/Wayland/display-manager del boot"
echo ""
echo "--- Socket activation ---"
echo "systemctl disable sshd.service"
echo "systemctl enable sshd.socket"
echo "  sshd solo arranca cuando alguien se conecta al puerto 22"
echo ""
echo "--- Medir el impacto ---"
echo "Antes:  systemd-analyze → 10.6s"
echo "Despues: systemd-analyze → 6.7s"
echo ""
echo "Siempre medir antes y despues de optimizar"
'
```

### Paso 3.3: debug-shell.service

```bash
docker compose exec debian-dev bash -c '
echo "=== debug-shell.service ==="
echo ""
echo "Proporciona un shell root en tty9 durante el boot"
echo "Util cuando el sistema no arranca correctamente"
echo ""
echo "--- Habilitar ---"
echo "systemctl enable debug-shell.service"
echo "  O desde GRUB: systemd.wants=debug-shell.service"
echo ""
echo "--- Usar ---"
echo "Durante el boot, cambiar a tty9: Ctrl+Alt+F9"
echo "Shell root SIN autenticacion"
echo ""
echo "--- RIESGO DE SEGURIDAD ---"
echo "Cualquiera con acceso fisico tiene root"
echo "SIEMPRE deshabilitar despues de resolver el problema:"
echo "  systemctl disable debug-shell.service"
echo ""
echo "--- Verificar si esta instalado ---"
ls /usr/lib/systemd/system/debug-shell.service 2>/dev/null || \
    ls /lib/systemd/system/debug-shell.service 2>/dev/null || \
    echo "debug-shell.service no encontrado"
'
```

### Paso 3.4: Parametros del kernel para systemd

```bash
docker compose exec debian-dev bash -c '
echo "=== Parametros del kernel para systemd ==="
echo ""
echo "Se agregan en GRUB al kernel command line"
echo ""
echo "| Parametro                    | Efecto                         |"
echo "|-----------------------------|--------------------------------|"
echo "| systemd.unit=rescue.target  | Arrancar en modo rescate       |"
echo "| systemd.unit=emergency      | Arrancar en modo emergencia    |"
echo "| systemd.mask=UNIT           | Enmascarar unidad (este boot)  |"
echo "| systemd.wants=UNIT          | Agregar dependencia (este boot)|"
echo "| systemd.log_level=debug     | Mas verbosidad                 |"
echo "| systemd.log_target=console  | Logs a la consola              |"
echo "| rd.break                    | Romper al initramfs            |"
echo "| init=/bin/bash              | Bypass completo de systemd     |"
echo ""
echo "--- Verificar cmdline actual ---"
if [[ -f /proc/cmdline ]]; then
    echo "cmdline: $(cat /proc/cmdline)"
    grep -oE "systemd\.[a-z_]+=[^ ]+" /proc/cmdline || \
        echo "(sin parametros de systemd)"
else
    echo "(no disponible en contenedor)"
fi
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. systemd decide que arrancar en orden: kernel cmdline
   (`systemd.unit=`) > `/etc/systemd/system/default.target` >
   `/usr/lib/systemd/system/default.target` > emergency.

2. El boot tiene tres fases: hardware→systemd, resolucion de
   dependencias (grafo), arranque paralelo respetando After/Before.

3. `systemd-analyze blame` muestra tiempos por unidad.
   `critical-chain` muestra la cadena que determina el tiempo total.
   `plot` genera un SVG con la linea de tiempo completa.

4. `systemd-analyze verify` valida archivos de unidad sin cargarlos.
   Detecta Type invalido, rutas no absolutas, valores erroneos.

5. Optimizaciones comunes: deshabilitar servicios innecesarios,
   usar socket activation, cambiar a multi-user.target en
   servidores. Siempre medir antes y despues.

6. `debug-shell.service` proporciona shell root en tty9 durante
   el boot. Es un riesgo de seguridad — deshabilitar despues
   de resolver el problema.
