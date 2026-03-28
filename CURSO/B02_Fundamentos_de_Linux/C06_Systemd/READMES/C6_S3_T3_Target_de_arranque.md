# T03 — Target de arranque

## Errata sobre los materiales originales

| # | Archivo | Línea | Error | Corrección |
|---|---------|-------|-------|------------|
| 1 | max.md | 52 | rescue.target descrito como activando "basic.target y sysinit.target" | rescue.target solo requiere `sysinit.target`, no `basic.target` |
| 2 | max.md | 78-79 | initramfs: "descomprime el root filesystem" y "es provista por GRUB" | initramfs contiene drivers para montar el root fs; es generada por `dracut`/`mkinitramfs`, no por GRUB (GRUB solo la carga en memoria) |
| 3 | max.md | 266 | Typo "arrance" | Corregido: "arranque" |

---

## Cómo systemd decide qué arrancar

Al iniciar, systemd sigue una **cadena de prioridad**. Usa la primera
opción que encuentre:

```
1. ¿Hay systemd.unit= en el kernel command line?
   Sí → usar ese target (override temporal)
   No ↓

2. ¿Existe /etc/systemd/system/default.target?
   Sí → usar ese symlink (configurado por el admin con set-default)
   No ↓

3. ¿Existe /usr/lib/systemd/system/default.target?
   Sí → usar ese symlink (proporcionado por la distribución)
   No → arrancar en emergency mode
```

```bash
# Ver qué target se usó en este boot:
systemctl get-default
# multi-user.target

# Ver el kernel command line actual:
cat /proc/cmdline
# BOOT_IMAGE=/vmlinuz-6.x root=/dev/sda1 ro quiet
# Si incluyera systemd.unit=rescue.target, habría arrancado en rescue
```

---

## El proceso de boot paso a paso

### Fase 1: Hardware → systemd (PID 1)

```
BIOS/UEFI  →  GRUB  →  kernel  →  initramfs  →  systemd (PID 1)
```

```bash
# El kernel ejecuta /sbin/init, que es un symlink a systemd:
ls -la /sbin/init
# /sbin/init → /usr/lib/systemd/systemd

# systemd (PID 1) es el primer proceso — ancestro de todos los demás
ps -p 1 -o comm=
# systemd
```

**initramfs** (initial RAM filesystem):
- Es una imagen generada por herramientas como `dracut` (RHEL/Fedora) o
  `mkinitramfs` (Debian/Ubuntu)
- Contiene módulos del kernel necesarios para encontrar y montar el
  filesystem raíz real (drivers de disco, LVM, LUKS, etc.)
- GRUB la carga en memoria junto con el kernel al arrancar
- Una vez montado el root real, transfiere el control a systemd

systemd lee en orden:
1. `/proc/cmdline` — parámetros del kernel (incluyendo `systemd.unit=`)
2. `/etc/systemd/system.conf` — configuración global del manager
3. `default.target` — el target que define qué arrancar

### Fase 2: Resolución de dependencias (transaction)

systemd construye un **grafo de dependencias** antes de activar unidades:

```bash
# 1. Lee default.target (ej: multi-user.target)
# 2. Resuelve Wants= y Requires= recursivamente → todas las unidades necesarias
# 3. Ordena según After= y Before= → define el orden de activación
# 4. Detecta dependencias circulares → rompe el ciclo con advertencia

# Ver la cadena completa:
systemctl list-dependencies default.target --no-pager
```

### Fase 3: Arranque paralelo

```
                    default.target
                         │
              ┌──────────┼──────────┐
              │          │          │
         local-fs    network    sockets
          .target     .target   .target
           │  │         │         │
        ┌──┘  └──┐   ┌──┘      ┌──┘
        │        │   │         │
     home.    var.  NM.     sshd.    ← arrancan en paralelo
     mount   mount service  socket     si no hay After/Before entre ellos
```

Las unidades que **no tienen relación de orden** entre sí arrancan
**simultáneamente**. Esto es lo que hace que systemd sea significativamente
más rápido que SysVinit, donde todo era secuencial.

---

## Analizar el boot

### systemd-analyze — Tiempo total

```bash
systemd-analyze
# Startup finished in 2.5s (kernel) + 5.1s (userspace) = 7.6s
# graphical.target reached after 5.0s in userspace
```

| Fase | Descripción |
|------|-------------|
| **kernel** | Desde que el hardware ejecuta el kernel hasta que lanza systemd |
| **initramfs** | Descompresión y montaje del root (puede estar incluido en kernel) |
| **userspace** | Desde que systemd inicia hasta que `default.target` está activo |

### systemd-analyze blame — Qué tardó más

```bash
systemd-analyze blame --no-pager
# 5.100s cloud-init.service
# 3.200s NetworkManager-wait-online.service
# 2.100s docker.service
# 1.500s apt-daily.service
# 0.800s nginx.service
```

> **Nota**: `blame` mide tiempo *wall-clock*, no CPU. Un servicio "lento"
> puede estar simplemente esperando (red, disco, otra unidad).

### systemd-analyze critical-chain — El camino más largo

```bash
systemd-analyze critical-chain --no-pager
# graphical.target @5.0s
# └─display-manager.service @4.2s +800ms
#   └─multi-user.target @4.1s
#     └─nginx.service @3.5s +600ms
#       └─network-online.target @3.4s
#         └─NetworkManager-wait-online.service @1.2s +2.2s
#           └─NetworkManager.service @1.0s +200ms
#             └─basic.target @0.9s
```

Cada línea: `unidad @cuándo_arrancó +cuánto_tardó`

La **cadena crítica** es el camino más largo de dependencias — determina
el tiempo mínimo del boot. La indentación muestra qué dependencia bloqueó
a cada unidad.

```bash
# Para un target específico:
systemd-analyze critical-chain multi-user.target --no-pager
```

### systemd-analyze plot — Gráfico SVG

```bash
# Genera un SVG con líneas de tiempo de cada unidad:
systemd-analyze plot > /tmp/boot.svg

# Abrir con navegador o visor de imágenes:
# Cada unidad = barra horizontal
#   Rojo   = activando
#   Verde  = activo
#   Gris   = fallido
# Barras que se solapan = arranque paralelo real
# La barra más a la derecha = define el tiempo total
```

### systemd-analyze verify — Verificar unidades

```bash
# Verificar errores de sintaxis y dependencias sin cargar la unidad:
systemd-analyze verify /etc/systemd/system/myapp.service

# Detecta:
# - Type inválido, valores erróneos
# - Rutas no absolutas en ExecStart
# - Dependencias circulares
# - Unidades referenciadas que no existen
```

### systemd-analyze dot — Diagrama de dependencias

```bash
# Genera formato DOT (Graphviz) con las dependencias:
systemd-analyze dot default.target 2>/dev/null | head -20

# Convertir a imagen PNG (requiere graphviz):
systemd-analyze dot default.target 2>/dev/null | dot -Tpng -o /tmp/boot-deps.png

# plot muestra TIEMPOS, dot muestra DEPENDENCIAS — son complementarios
```

---

## Optimizar el boot

### Paso 1: Identificar cuellos de botella

```bash
# Los 10 servicios más lentos:
systemd-analyze blame --no-pager | head -10

# La cadena crítica:
systemd-analyze critical-chain --no-pager
```

Culpables comunes:

| Servicio | Causa | Solución |
|----------|-------|----------|
| `NetworkManager-wait-online` | Espera a que la red esté "up" (3-30s) | Deshabilitar si no se necesita `network-online.target` |
| `cloud-init.service` | Solo útil en cloud; inútil en bare metal | `disable` + `mask` |
| `apt-daily.service` | Busca actualizaciones al boot | Deshabilitar timers |
| `docker.service` | Inicialización de contenedores | Usar `docker.socket` |
| `plymouth-quit-wait.service` | Espera a que la GUI arranque | `mask` si no se necesita splash |

### Paso 2: Aplicar optimizaciones

```bash
# Deshabilitar servicios innecesarios:
sudo systemctl disable NetworkManager-wait-online.service
sudo systemctl disable apt-daily.timer apt-daily-upgrade.timer
sudo systemctl mask plymouth-quit-wait.service
sudo systemctl disable cloud-init.service && sudo systemctl mask cloud-init.service

# Cambiar de graphical a multi-user en servidores:
sudo systemctl set-default multi-user.target
# Elimina X11/Wayland/display-manager del boot

# Socket activation (arranque on-demand):
sudo systemctl disable sshd.service
sudo systemctl enable sshd.socket
# sshd solo arranca cuando alguien se conecta al puerto 22
```

### Paso 3: Medir el impacto

```bash
# ANTES de optimizar:
systemd-analyze
# Startup finished in 2.5s (kernel) + 8.1s (userspace) = 10.6s

# DESPUÉS:
systemd-analyze
# Startup finished in 2.5s (kernel) + 4.2s (userspace) = 6.7s
```

> Siempre medir antes y después. Si un cambio no mejora el tiempo, revertirlo.

---

## Kernel command line y systemd

Parámetros que se agregan en GRUB a la línea del kernel (ver T02 para el
procedimiento desde GRUB):

| Parámetro | Efecto |
|-----------|--------|
| `systemd.unit=TARGET` | Override temporal del target de arranque |
| `systemd.mask=UNIT` | Enmascarar una unidad para este boot |
| `systemd.wants=UNIT` | Agregar una dependencia extra para este boot |
| `systemd.log_level=debug` | Logs muy verbosos durante boot |
| `systemd.log_target=console` | Enviar logs directamente a la consola |
| `rd.break` | Romper en initramfs (antes de montar root) |
| `init=/bin/bash` | Bypass total de systemd — shell directo del kernel |

```bash
# Ver cmdline del boot actual:
cat /proc/cmdline

# Buscar overrides de systemd:
grep -oE 'systemd\.[a-z_]+=[^ ]+' /proc/cmdline || echo "Ninguno"
```

---

## debug-shell.service — Shell de emergencia

```bash
# Proporciona un shell root en tty9 accesible durante el boot
# Útil cuando el sistema no puede arrancar y systemctl no funciona

# Habilitar (temporalmente):
sudo systemctl enable debug-shell.service
# O desde GRUB: systemd.wants=debug-shell.service

# Durante boot: Ctrl+Alt+F9 → shell root SIN autenticación

# RIESGO DE SEGURIDAD: acceso físico = root inmediato
# Deshabilitar INMEDIATAMENTE después de resolver el problema:
sudo systemctl disable debug-shell.service
```

---

## Troubleshooting quick-reference

| Problema | Diagnóstico | Solución |
|----------|-------------|----------|
| Boot extremadamente lento | `systemd-analyze blame` | Identificar y deshabilitar servicio más lento |
| "A start job is running for..." | `journalctl -b` | Buscar el servicio que no responde |
| Unidades fallidas | `systemctl --failed` | Ver errores con `journalctl -u UNIT` |
| systemd no arranca (PID 1 roto) | `rd.break` en cmdline | Recovery shell en initramfs |
| Aplicación no funciona post-boot | `journalctl -u NOMBRE.service -b` | Logs específicos del servicio |

```bash
# Logs del boot actual:
journalctl -b --no-pager

# Logs de un servicio específico:
journalctl -u nginx.service -b --no-pager

# Logs del boot anterior (si hay persistencia):
journalctl -b -1 --no-pager
```

---

## Ejercicios

### Ejercicio 1 — Cadena de decisión del boot

```bash
# Verificar cómo systemd eligió el target actual:

# 1. ¿Hay override en kernel cmdline?
cat /proc/cmdline 2>/dev/null
grep -oE 'systemd\.unit=[^ ]+' /proc/cmdline 2>/dev/null || echo "Sin override"

# 2. ¿Existe default.target en /etc/?
ls -la /etc/systemd/system/default.target 2>/dev/null

# 3. ¿Existe default.target en /usr/lib/?
ls -la /usr/lib/systemd/system/default.target 2>/dev/null
```

<details><summary>Predicción</summary>

- En un sistema normal, no habrá `systemd.unit=` en cmdline
- `default.target` existirá en `/etc/systemd/system/` como un symlink
  creado por `systemctl set-default`
- systemd usó el paso 2 de la cadena (symlink en /etc/)
- Si el symlink en /etc/ no existiera, usaría el de /usr/lib/ (paso 3)
- Si ninguno existiera, arrancaría en emergency mode (paso 4)

</details>

### Ejercicio 2 — Las tres fases del boot

```bash
# Verificar cada fase:

# Fase 1: ¿/sbin/init apunta a systemd?
ls -la /sbin/init

# Fase 2: ¿Cuántas unidades en el grafo de dependencias?
systemctl list-dependencies "$(systemctl get-default)" --plain --no-pager | wc -l

# Fase 3: ¿Hay arranque paralelo? (verificar con tiempos)
systemd-analyze 2>/dev/null || echo "(no disponible en contenedor)"
```

<details><summary>Predicción</summary>

- `/sbin/init` será un symlink a `/usr/lib/systemd/systemd` (o
  `/lib/systemd/systemd` en Debian)
- El grafo de dependencias tendrá decenas o cientos de unidades —
  cada `Wants=` y `Requires=` agrega más al grafo recursivamente
- `systemd-analyze` mostrará que el tiempo de userspace es menor que
  la suma de `blame` porque muchas unidades arrancaron en paralelo
- En un contenedor Docker, `systemd-analyze` podría no estar disponible

</details>

### Ejercicio 3 — systemd-analyze blame

```bash
# ¿Cuáles son los 10 servicios más lentos?
systemd-analyze blame --no-pager 2>/dev/null | head -10

# ¿Cuál es el servicio más lento del sistema?
systemd-analyze blame --no-pager 2>/dev/null | head -1
```

<details><summary>Predicción</summary>

- El servicio más lento típicamente será `NetworkManager-wait-online.service`
  (2-30 segundos esperando la red) o `cloud-init.service` (en instancias cloud)
- Los tiempos son wall-clock: un servicio que muestra 5s no usó 5s de CPU
  necesariamente — pudo estar esperando red, disco, u otra dependencia
- La suma de todos los tiempos será **mayor** que el tiempo total de boot
  porque muchos servicios arrancan en paralelo

</details>

### Ejercicio 4 — Cadena crítica

```bash
# Ver la cadena crítica completa:
systemd-analyze critical-chain --no-pager 2>/dev/null

# ¿Qué unidad está al final de la cadena (la primera en arrancar)?
systemd-analyze critical-chain --no-pager 2>/dev/null | tail -3
```

<details><summary>Predicción</summary>

- La cadena empezará con el default target (graphical o multi-user) y
  bajará hasta `basic.target` o `sysinit.target`
- Los `+Xms` grandes indican los cuellos de botella reales
- Al final de la cadena (la unidad más profunda) estará algo como
  `basic.target` o `sysinit.target` — los primeros targets en activarse
- Un servicio con `+2.2s` en la cadena crítica es un candidato directo
  para optimización — cada segundo que se reduzca ahí se reduce del
  boot total

</details>

### Ejercicio 5 — systemd-analyze verify

```bash
# Crear un archivo con errores intencionados y verificarlo:
cat > /tmp/test-verify.service << 'EOF'
[Unit]
Description=Test Service

[Service]
Type=invalid-type
ExecStart=no-absolute-path
Restart=maybe

[Install]
WantedBy=multi-user.target
EOF

systemd-analyze verify /tmp/test-verify.service 2>&1
rm /tmp/test-verify.service
```

<details><summary>Predicción</summary>

- `verify` reportará al menos 3 errores:
  - `Type=invalid-type` no es un tipo válido (los válidos son: simple, exec,
    forking, oneshot, dbus, notify, idle)
  - `ExecStart=no-absolute-path` — ExecStart requiere ruta absoluta
    (empezar con `/`)
  - `Restart=maybe` — valor inválido (los válidos son: no, on-success,
    on-failure, on-abnormal, on-abort, on-watchdog, always)
- `verify` es una herramienta de validación estática — no carga ni ejecuta
  la unidad, solo analiza el archivo

</details>

### Ejercicio 6 — Generar plot SVG

```bash
# Generar el gráfico de boot (si systemd es PID 1):
systemd-analyze plot > /tmp/boot.svg 2>/dev/null && \
    echo "SVG generado en /tmp/boot.svg ($(wc -c < /tmp/boot.svg) bytes)" || \
    echo "(no disponible — requiere systemd como PID 1)"

# Si se generó, ver cuántas unidades aparecen:
grep -c "<text" /tmp/boot.svg 2>/dev/null || true
```

<details><summary>Predicción</summary>

- Si systemd es PID 1, se generará un SVG de varios KB con barras
  horizontales para cada unidad
- Barras rojas = unidad activándose; verdes = activa; solapadas = paralelas
- La barra más a la derecha define el tiempo total del boot
- En un contenedor Docker, probablemente no se genere (systemd no es PID 1)
- El SVG se puede abrir en cualquier navegador web para visualizarlo

</details>

### Ejercicio 7 — Identificar candidatos de optimización

```bash
# Lista de servicios que podrían ser innecesarios:
for svc in cloud-init.service \
           NetworkManager-wait-online.service \
           plymouth-quit-wait.service \
           apt-daily.timer \
           apt-daily-upgrade.timer \
           snapd.service; do
    STATUS=$(systemctl is-enabled "$svc" 2>/dev/null || echo "not-found")
    echo "$svc → $STATUS"
done
```

<details><summary>Predicción</summary>

- Algunos servicios estarán `enabled`, otros `not-found` (depende de la
  distro y paquetes instalados)
- En un servidor bare metal: `cloud-init` y `plymouth` son candidatos
  a deshabilitar
- En un desktop: `NetworkManager-wait-online` podría deshabilitarse si
  ningún servicio necesita `network-online.target` al boot
- `not-found` significa que el paquete no está instalado — no hay nada
  que optimizar ahí

</details>

### Ejercicio 8 — debug-shell.service

```bash
# Verificar si debug-shell.service existe:
systemctl cat debug-shell.service 2>/dev/null || \
    echo "debug-shell.service no encontrado"

# ¿Está habilitado? (debería estar deshabilitado por seguridad)
systemctl is-enabled debug-shell.service 2>/dev/null || \
    echo "No disponible"
```

<details><summary>Predicción</summary>

- `debug-shell.service` existirá en la mayoría de distribuciones con
  systemd (está en el paquete base)
- Debería estar **disabled** — si estuviera enabled, cualquier persona
  con acceso físico tendría un shell root en tty9 sin contraseña
- El servicio arranca un `getty` en `/dev/tty9` como root sin
  autenticación — es solo para diagnóstico temporal

</details>

### Ejercicio 9 — Socket activation vs servicio directo

```bash
# Comparar sshd como servicio vs como socket:

# ¿Está habilitado como servicio?
systemctl is-enabled sshd.service 2>/dev/null || \
    systemctl is-enabled ssh.service 2>/dev/null || echo "SSH no encontrado"

# ¿Existe el socket?
systemctl cat sshd.socket 2>/dev/null || \
    systemctl cat ssh.socket 2>/dev/null || echo "Socket no encontrado"

# Comparar: ¿cuánto tardó sshd en arrancar?
systemd-analyze blame 2>/dev/null | grep -i ssh || echo "(no disponible)"
```

<details><summary>Predicción</summary>

- En la mayoría de sistemas, sshd estará habilitado como **servicio**
  (arranca al boot, siempre activo)
- El archivo del socket existirá pero estará disabled — es una alternativa
  de activación por demanda
- Con socket activation, sshd solo arrancaría cuando alguien se conecta
  al puerto 22, eliminándolo del camino crítico del boot
- El trade-off: la primera conexión SSH tarda un poco más (hay que
  arrancar sshd), pero el boot es más rápido

</details>

### Ejercicio 10 — Diagnóstico: boot lento

El boot tarda 45 segundos. `systemd-analyze critical-chain` muestra:

```
multi-user.target @44.5s
└─myapp.service @12.3s +32.2s
  └─network-online.target @12.2s
    └─NetworkManager-wait-online.service @2.0s +10.2s
      └─NetworkManager.service @1.5s +500ms
        └─basic.target @1.4s
```

¿Cuáles son los dos cuellos de botella y qué harías para cada uno?

<details><summary>Predicción</summary>

Los dos cuellos de botella son:

1. **`myapp.service` (+32.2s)**: es el mayor cuello — tarda 32 segundos
   en arrancar. Acciones posibles:
   - Investigar por qué tarda tanto: `journalctl -u myapp.service -b`
   - ¿Está haciendo algo síncrono al arrancar (ej: migración de DB)?
   - Si no necesita network-online, cambiar a `After=network.target`
   - Si es Type=simple pero tarda en estar listo, considerar Type=notify

2. **`NetworkManager-wait-online.service` (+10.2s)**: espera 10 segundos
   la red. Acciones posibles:
   - Si myapp realmente necesita red al arrancar, este tiempo es
     inevitable
   - Si no la necesita, cambiar a `After=network.target` (sin `-online`)
     y quitar `Wants=network-online.target`
   - Optimizar la configuración de red (IP estática en vez de DHCP)

- El tiempo total sería: basic(1.4s) + NM(0.5s) + wait-online(10.2s) +
  myapp(32.2s) ≈ 44.5s — la cadena se recorre secuencialmente
- Reducir myapp de 32s a 2s reduciría el boot a ~14s

</details>
