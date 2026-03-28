# Lab — Operaciones

## Objetivo

Dominar las operaciones de APT: el ciclo de vida completo de un
paquete (install, remove, purge, autoremove), la diferencia entre
upgrade y full-upgrade, y las opciones para operaciones no
interactivas en scripts y Dockerfiles.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Ciclo de vida de un paquete

### Objetivo

Observar cada etapa desde la instalacion hasta la eliminacion
completa, incluyendo la diferencia entre remove y purge.

### Paso 1.1: Actualizar indices

```bash
docker compose exec debian-dev bash -c '
echo "=== apt update ==="
apt-get update -qq 2>&1 | tail -5
echo ""
echo "--- Que hizo ---"
echo "Descargo los indices de TODOS los repositorios configurados"
echo "NO instalo nada — solo actualizo la lista de disponibles"
echo ""
echo "Hit:  indice sin cambios"
echo "Get:  indice descargado (nuevo o actualizado)"
echo "Ign:  ignorado"
echo "Err:  error"
'
```

### Paso 1.2: Instalar un paquete

```bash
docker compose exec debian-dev bash -c '
echo "=== Instalar cowsay ==="
apt-get install -y -qq cowsay 2>&1
echo ""
echo "--- Verificar ---"
echo "Estado en dpkg:"
dpkg -l cowsay 2>/dev/null | tail -1
echo ""
echo "El prefijo ii significa:"
echo "  i = desired: install"
echo "  i = status: installed"
echo ""
echo "Archivos instalados:"
dpkg -L cowsay | wc -l
echo "archivos"
echo ""
echo "Usar cowsay:"
/usr/games/cowsay "Hola desde el lab" 2>/dev/null || echo "(cowsay no esta en PATH, esta en /usr/games/)"
'
```

### Paso 1.3: remove — eliminar sin config

Prediccion: remove elimina los binarios pero deja la configuracion.

```bash
docker compose exec debian-dev bash -c '
echo "=== apt remove ==="
apt-get remove -y -qq cowsay 2>&1
echo ""
echo "--- Estado despues de remove ---"
dpkg -l cowsay 2>/dev/null | tail -1
echo ""
echo "El prefijo rc significa:"
echo "  r = desired: remove"
echo "  c = status: config-files present"
echo ""
echo "El binario ya no existe:"
ls /usr/games/cowsay 2>/dev/null || echo "  /usr/games/cowsay: no existe"
echo ""
echo "Pero la configuracion sigue:"
dpkg -L cowsay 2>/dev/null | head -5 || echo "  (archivos listados en dpkg aun)"
'
```

### Paso 1.4: purge — eliminar todo

```bash
docker compose exec debian-dev bash -c '
echo "=== apt purge ==="
apt-get purge -y -qq cowsay 2>&1
echo ""
echo "--- Estado despues de purge ---"
result=$(dpkg -l cowsay 2>/dev/null | tail -1)
if [ -z "$result" ]; then
    echo "  cowsay ya no aparece en dpkg -l"
else
    echo "  $result"
    echo "  pn = purged, not-installed"
fi
echo ""
echo "=== remove vs purge ==="
printf "%-12s %-15s %-15s\n" "Accion" "Binarios" "Config /etc"
printf "%-12s %-15s %-15s\n" "-----------" "--------------" "--------------"
printf "%-12s %-15s %-15s\n" "remove" "Eliminados" "CONSERVADOS"
printf "%-12s %-15s %-15s\n" "purge" "Eliminados" "ELIMINADOS"
echo ""
echo "Ningun elimina datos de usuario (/home, /var/lib/...)"
'
```

### Paso 1.5: autoremove — limpiar huerfanos

```bash
docker compose exec debian-dev bash -c '
echo "=== Dependencias huerfanas ==="
echo ""
echo "Cuando instalas un paquete, sus dependencias se marcan auto."
echo "Al eliminar el paquete, las dependencias quedan huerfanas."
echo ""
echo "Paquetes que autoremove eliminaria:"
apt-get autoremove --simulate 2>/dev/null | grep "^Remv" | head -10
count=$(apt-get autoremove --simulate 2>/dev/null | grep "^Remv" | wc -l)
echo ""
echo "Total: $count paquetes huerfanos"
echo ""
echo "autoremove:         elimina binarios, conserva config"
echo "autoremove --purge: elimina binarios Y config"
'
```

---

## Parte 2 — Upgrades

### Objetivo

Entender la diferencia entre upgrade y full-upgrade, y como
simular operaciones antes de ejecutarlas.

### Paso 2.1: Simular un upgrade

```bash
docker compose exec debian-dev bash -c '
echo "=== Simular apt upgrade ==="
echo ""
echo "--- apt upgrade --simulate ---"
apt-get upgrade --simulate 2>/dev/null | tail -10
echo ""
echo "upgrade es CONSERVADOR:"
echo "  - Actualiza paquetes a versiones nuevas"
echo "  - NO instala paquetes nuevos"
echo "  - NO elimina paquetes existentes"
echo "  - Si un upgrade requiere instalar/eliminar, NO se hace"
'
```

### Paso 2.2: Simular un full-upgrade

```bash
docker compose exec debian-dev bash -c '
echo "=== Simular apt full-upgrade ==="
echo ""
echo "--- apt full-upgrade --simulate ---"
apt-get dist-upgrade --simulate 2>/dev/null | tail -10
echo ""
echo "full-upgrade es MAS AGRESIVO:"
echo "  - Todo lo que hace upgrade"
echo "  - PUEDE instalar paquetes nuevos"
echo "  - PUEDE eliminar paquetes"
echo ""
echo "=== Cuando usar cada uno ==="
echo "upgrade:      actualizaciones rutinarias de seguridad"
echo "full-upgrade: migrar a nueva release o cuando upgrade"
echo "              deja paquetes sin actualizar"
echo ""
echo "Nota: apt full-upgrade = apt-get dist-upgrade"
'
```

### Paso 2.3: La opcion --simulate

```bash
docker compose exec debian-dev bash -c '
echo "=== --simulate / -s: dry run ==="
echo ""
echo "Simular sin ejecutar (no necesita sudo):"
echo ""
echo "--- Simular instalar build-essential ---"
apt-get install --simulate build-essential 2>/dev/null | grep -E "^(Inst|Remv|Conf)" | head -10
echo "..."
echo ""
total=$(apt-get install --simulate build-essential 2>/dev/null | grep "^Inst" | wc -l)
echo "Se instalarian: $total paquetes"
echo ""
echo "Util para verificar ANTES de ejecutar:"
echo "  - Cuantos paquetes se instalarian"
echo "  - Cuanto espacio se usaria"
echo "  - Si hay conflictos"
'
```

---

## Parte 3 — Operaciones no interactivas

### Objetivo

Usar las opciones correctas para scripts y Dockerfiles donde
no hay terminal interactivo.

### Paso 3.1: DEBIAN_FRONTEND

```bash
docker compose exec debian-dev bash -c '
echo "=== DEBIAN_FRONTEND ==="
echo ""
echo "Controla como debconf hace preguntas durante la instalacion:"
echo ""
echo "  dialog       → interfaz ncurses (default en terminal)"
echo "  readline     → preguntas en texto plano"
echo "  noninteractive → NO hace preguntas (usa defaults)"
echo "  teletype     → preguntas minimas"
echo ""
echo "Para scripts y Dockerfiles:"
echo "  DEBIAN_FRONTEND=noninteractive apt-get install -y ..."
echo ""
echo "En Dockerfiles:"
echo "  ENV DEBIAN_FRONTEND=noninteractive"
echo "  RUN apt-get update && apt-get install -y pkg"
echo ""
echo "Valor actual: ${DEBIAN_FRONTEND:-dialog (default)}"
'
```

### Paso 3.2: Flags para automatizacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Flags para scripts ==="
echo ""
echo "-y / --yes"
echo "  Responder yes automaticamente a confirmaciones"
echo ""
echo "-q / --quiet"
echo "  Reducir output (usar -qq para output minimo)"
echo ""
echo "--no-install-recommends"
echo "  No instalar paquetes recomendados"
echo "  Los Recommends mejoran funcionalidad pero no son necesarios"
echo "  En servidores y contenedores, omitirlos ahorra espacio"
echo ""
echo "=== Patron para Dockerfiles ==="
echo ""
echo "RUN apt-get update && \\"
echo "    apt-get install -y --no-install-recommends \\"
echo "        curl ca-certificates && \\"
echo "    rm -rf /var/lib/apt/lists/*"
echo ""
echo "El rm -rf /var/lib/apt/lists/* limpia los indices"
echo "para reducir el tamano de la capa Docker"
'
```

### Paso 3.3: fix-broken

```bash
docker compose exec debian-dev bash -c '
echo "=== Reparar paquetes rotos ==="
echo ""
echo "Si una instalacion fallo a medias:"
echo ""
echo "  sudo apt --fix-broken install"
echo "  sudo dpkg --configure -a"
echo ""
echo "Estado actual del sistema:"
broken=$(dpkg -l | grep -E "^(iU|iF|iH)" | wc -l)
echo "  Paquetes en estado broken: $broken"
echo ""
if [ "$broken" -gt 0 ]; then
    echo "Paquetes problematicos:"
    dpkg -l | grep -E "^(iU|iF|iH)"
else
    echo "  (sistema limpio — ningun paquete roto)"
fi
echo ""
echo "--- Prefijos de estado dpkg ---"
echo "ii = correctamente instalado"
echo "iU = unpacked (falta configurar)"
echo "iF = half-configured (configuracion fallo)"
echo "iH = half-installed (instalacion fallo)"
'
```

### Paso 3.4: Buscar y consultar

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar paquetes ==="
echo ""
echo "--- apt search (formato legible) ---"
apt search "^curl$" 2>/dev/null | head -5
echo ""
echo "--- apt-cache search (formato simple) ---"
apt-cache search "^curl$" 2>/dev/null | head -5
echo ""
echo "=== A que paquete pertenece un archivo ==="
dpkg -S /usr/bin/curl 2>/dev/null
dpkg -S /usr/bin/env 2>/dev/null
echo ""
echo "=== Dependencias inversas (quien depende de curl) ==="
apt-cache rdepends curl 2>/dev/null | head -8
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
apt-get purge -y -qq cowsay 2>/dev/null
apt-get autoremove -y -qq 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. El ciclo de vida de un paquete APT: `apt update` (indices) →
   `apt install` (instalar) → `apt remove` (quitar binarios) o
   `apt purge` (quitar binarios + config) → `apt autoremove`
   (limpiar dependencias huerfanas).

2. `remove` deja archivos de configuracion en `/etc` (estado
   **rc** en dpkg). `purge` elimina todo (estado **pn**).

3. `upgrade` es conservador: no instala ni elimina paquetes.
   `full-upgrade` puede instalar nuevos y eliminar existentes
   para resolver dependencias cambiadas.

4. `--simulate` permite ver que haria un comando **sin
   ejecutarlo**. No necesita sudo.

5. Para scripts y Dockerfiles: usar `apt-get` con
   `DEBIAN_FRONTEND=noninteractive`, `-y`, y
   `--no-install-recommends`. Limpiar con
   `rm -rf /var/lib/apt/lists/*`.

6. `apt --fix-broken install` y `dpkg --configure -a` reparan
   instalaciones interrumpidas.
