# Lab — Targets vs runlevels

## Objetivo

Entender la transicion de runlevels (SysVinit) a targets (systemd):
el mapeo entre ambos modelos, los targets principales y de
sincronizacion, la cadena de boot, y las ventajas del modelo de
targets (paralelo, multiple, extensible).

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Runlevels y mapeo

### Objetivo

Entender los runlevels de SysVinit y como se mapean a targets
de systemd.

### Paso 1.1: Runlevels de SysVinit

```bash
docker compose exec debian-dev bash -c '
echo "=== Runlevels de SysVinit ==="
echo ""
echo "| Runlevel | Descripcion                              |"
echo "|----------|------------------------------------------|"
echo "| 0        | Halt (apagar)                            |"
echo "| 1        | Single user (mantenimiento, sin red)     |"
echo "| 2        | Multi-user sin NFS (Debian: completo)    |"
echo "| 3        | Multi-user completo con red (sin GUI)    |"
echo "| 4        | No usado (reservado)                     |"
echo "| 5        | Multi-user con GUI                       |"
echo "| 6        | Reboot                                   |"
echo ""
echo "Problemas del modelo:"
echo "  1. Solo UN runlevel activo a la vez"
echo "  2. Arranque SECUENCIAL (S10 espera a S09)"
echo "  3. Sin dependencias declarativas (orden numerico)"
echo "  4. Dificil de extender (symlinks manuales)"
echo "  5. Runlevels 2/3/4 casi identicos pero duplicados"
'
```

### Paso 1.2: Mapeo runlevel → target

```bash
docker compose exec debian-dev bash -c '
echo "=== Mapeo runlevel → target ==="
echo ""
echo "| Runlevel | Target systemd      | Descripcion        |"
echo "|----------|---------------------|--------------------|"
echo "| 0        | poweroff.target     | Apagar             |"
echo "| 1        | rescue.target       | Single-user        |"
echo "| 2, 3, 4  | multi-user.target   | Servidor sin GUI   |"
echo "| 5        | graphical.target    | Desktop con GUI    |"
echo "| 6        | reboot.target       | Reiniciar          |"
echo ""
echo "--- Aliases de compatibilidad ---"
for i in 0 1 2 3 4 5 6; do
    TARGET=$(readlink /usr/lib/systemd/system/runlevel${i}.target 2>/dev/null || \
             readlink /lib/systemd/system/runlevel${i}.target 2>/dev/null)
    if [[ -n "$TARGET" ]]; then
        printf "  runlevel%d.target → %s\n" "$i" "$TARGET"
    else
        printf "  runlevel%d.target → (no encontrado)\n" "$i"
    fi
done
echo ""
echo "Nota: runlevels 2, 3 y 4 son EQUIVALENTES en systemd"
echo "La distincion SysVinit desaparece — la red se gestiona"
echo "con network.target independientemente del target principal"
'
```

### Paso 1.3: Diferencias fundamentales

```bash
docker compose exec debian-dev bash -c '
echo "=== Targets vs runlevels ==="
echo ""
echo "| Aspecto       | Runlevels            | Targets              |"
echo "|---------------|---------------------|----------------------|"
echo "| Activos       | Solo uno a la vez   | Multiples            |"
echo "| Arranque      | Secuencial          | Paralelo             |"
echo "| Dependencias  | Implicitas (numero) | Explicitas (After)   |"
echo "| Extensibilidad| Fija (0-6)          | Ilimitada (custom)   |"
echo "| Granularidad  | Gruesa              | Fina (por servicio)  |"
echo ""
echo "--- Multiples targets activos ---"
echo "En systemd, VARIOS targets estan activos simultaneamente:"
echo ""
echo "Targets que estarian activos en un servidor tipico:"
echo "  basic.target"
echo "  local-fs.target"
echo "  multi-user.target"
echo "  network.target"
echo "  sockets.target"
echo "  timers.target"
echo "  sysinit.target"
echo "  ..."
echo ""
echo "En SysVinit, solo un runlevel (ej: 3)"
'
```

---

## Parte 2 — Targets principales

### Objetivo

Explorar los targets principales de systemd y sus archivos.

### Paso 2.1: poweroff, reboot, halt

```bash
docker compose exec debian-dev bash -c '
echo "=== Targets terminales ==="
echo ""
echo "--- poweroff.target ---"
cat /usr/lib/systemd/system/poweroff.target 2>/dev/null || \
    cat /lib/systemd/system/poweroff.target 2>/dev/null || \
    echo "(no encontrado)"
echo ""
echo "--- reboot.target ---"
cat /usr/lib/systemd/system/reboot.target 2>/dev/null || \
    cat /lib/systemd/system/reboot.target 2>/dev/null || \
    echo "(no encontrado)"
echo ""
echo "Estos targets declaran Conflicts= con servicios normales"
echo "→ primero se detiene todo, luego se apaga/reinicia"
echo ""
echo "Comandos equivalentes:"
echo "  systemctl poweroff  = shutdown -h now = init 0"
echo "  systemctl reboot    = shutdown -r now = init 6"
echo "  systemctl halt      = halt"
'
```

### Paso 2.2: rescue y emergency

```bash
docker compose exec debian-dev bash -c '
echo "=== rescue.target vs emergency.target ==="
echo ""
echo "--- rescue.target (runlevel 1) ---"
cat /usr/lib/systemd/system/rescue.target 2>/dev/null || \
    cat /lib/systemd/system/rescue.target 2>/dev/null || \
    echo "(no encontrado)"
echo ""
echo "rescue.target:"
echo "  sysinit.target completado (udev, tmpfiles, journal)"
echo "  Filesystem raiz montado read-write"
echo "  Sin red, sin servicios de usuario"
echo "  Pide contrasena de root → shell interactivo"
echo ""
echo "--- emergency.target ---"
cat /usr/lib/systemd/system/emergency.target 2>/dev/null || \
    cat /lib/systemd/system/emergency.target 2>/dev/null || \
    echo "(no encontrado)"
echo ""
echo "emergency.target:"
echo "  sysinit.target NO ejecutado"
echo "  Filesystem raiz en READ-ONLY"
echo "  Casi nada corriendo"
echo "  Shell root directo"
echo ""
echo "| Caracteristica | rescue     | emergency   |"
echo "|----------------|------------|-------------|"
echo "| Filesystem     | read-write | read-only   |"
echo "| sysinit        | completado | no ejecutado|"
echo "| udev/journald  | activo     | no activo   |"
echo "| Cuando usar    | Config     | Boot/FS roto|"
'
```

### Paso 2.3: multi-user y graphical

```bash
docker compose exec debian-dev bash -c '
echo "=== multi-user.target y graphical.target ==="
echo ""
echo "--- multi-user.target (servidores) ---"
cat /usr/lib/systemd/system/multi-user.target 2>/dev/null || \
    cat /lib/systemd/system/multi-user.target 2>/dev/null || \
    echo "(no encontrado)"
echo ""
echo "multi-user.target:"
echo "  Red configurada"
echo "  Todos los servicios del sistema arrancados"
echo "  Login por consola y SSH"
echo "  Sin interfaz grafica"
echo ""
echo "--- graphical.target (desktops) ---"
cat /usr/lib/systemd/system/graphical.target 2>/dev/null || \
    cat /lib/systemd/system/graphical.target 2>/dev/null || \
    echo "(no encontrado)"
echo ""
echo "graphical.target DEPENDE de multi-user.target"
echo "Agrega: display manager (GDM/SDDM), servidor grafico"
echo ""
echo "--- Default target ---"
DEFAULT=$(readlink /etc/systemd/system/default.target 2>/dev/null || \
          readlink /usr/lib/systemd/system/default.target 2>/dev/null || \
          echo "no configurado")
echo "default.target → $DEFAULT"
'
```

---

## Parte 3 — Targets de sincronizacion

### Objetivo

Entender los targets de sincronizacion y la cadena de boot.

### Paso 3.1: Cadena de boot

```bash
docker compose exec debian-dev bash -c '
echo "=== Cadena de boot ==="
echo ""
echo "local-fs-pre.target"
echo "        │"
echo "local-fs.target ─── swap.target"
echo "        │"
echo "   sysinit.target      ← udev, tmpfiles, journal, swap"
echo "        │"
echo "   basic.target        ← D-Bus, sockets base, timers"
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
echo "Los targets de sincronizacion NO representan estados del sistema"
echo "sino PUNTOS de sincronizacion durante el boot"
'
```

### Paso 3.2: network.target vs network-online.target

Antes de ejecutar, predice: que target necesita un servicio
que conecta a una base de datos remota al arrancar?

```bash
docker compose exec debian-dev bash -c '
echo "=== network.target vs network-online.target ==="
echo ""
echo "--- network.target ---"
echo "Interfaces configuradas (IP asignada)"
echo "PUEDE no haber conectividad real (DHCP en progreso)"
echo "Suficiente para servicios que ESCUCHAN (bind a IP/puerto)"
echo "  After=network.target"
echo ""
echo "--- network-online.target ---"
echo "Conectividad real verificada (rutas, DNS)"
echo "Necesario para servicios que CONECTAN a hosts remotos"
echo "  After=network-online.target"
echo "  Wants=network-online.target    ← NECESARIO (no se activa solo)"
echo ""
echo "--- Verificar existencia ---"
ls -la /usr/lib/systemd/system/network.target \
       /usr/lib/systemd/system/network-online.target 2>/dev/null || \
ls -la /lib/systemd/system/network.target \
       /lib/systemd/system/network-online.target 2>/dev/null
'
```

### Paso 3.3: Explorar targets instalados

```bash
docker compose exec debian-dev bash -c '
echo "=== Targets instalados ==="
echo ""
echo "--- Listar todos los targets ---"
ls /usr/lib/systemd/system/*.target 2>/dev/null | \
    xargs -I{} basename {} | sort
echo ""
echo "--- Contar ---"
COUNT=$(ls /usr/lib/systemd/system/*.target 2>/dev/null | wc -l)
echo "Total: $COUNT targets"
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== Targets en AlmaLinux ==="
echo ""
COUNT=$(ls /usr/lib/systemd/system/*.target 2>/dev/null | wc -l)
echo "Total: $COUNT targets"
echo ""
echo "--- Targets exclusivos de RHEL ---"
for t in /usr/lib/systemd/system/*.target; do
    [[ -f "$t" ]] || continue
    BASE=$(basename "$t")
    if [[ ! -f "/usr/lib/systemd/system/$BASE" ]]; then
        echo "  $BASE"
    fi
done | head -5
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los **runlevels** de SysVinit (0-6) se mapean a targets de
   systemd: 0→poweroff, 1→rescue, 2/3/4→multi-user, 5→graphical,
   6→reboot. Los symlinks `runlevelN.target` mantienen compatibilidad.

2. A diferencia de los runlevels, **multiples targets** pueden
   estar activos simultaneamente. El arranque es **paralelo**
   donde las dependencias lo permiten.

3. **rescue.target** tiene sysinit completado y filesystem rw.
   **emergency.target** es mas restrictivo: sin sysinit,
   filesystem en read-only. Usar emergency cuando rescue no arranca.

4. **multi-user.target** es el default para servidores (sin GUI).
   **graphical.target** extiende multi-user agregando display
   manager.

5. **network.target** = interfaces con IP (para servicios que
   escuchan). **network-online.target** = conectividad real
   (para servicios que conectan). Usar `Wants=` con el segundo.

6. Los targets de sincronizacion (sysinit, basic, sockets, timers)
   son puntos de referencia en la cadena de boot, no estados
   del sistema.
