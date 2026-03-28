# Lab — apt vs apt-get

## Objetivo

Entender la familia de herramientas APT (dpkg, apt-get, apt-cache,
apt-mark, apt), comparar apt y apt-get en la practica, y saber
cuando usar cada uno.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — La familia APT

### Objetivo

Entender la jerarquia de herramientas y que hace cada una.

### Paso 1.1: La jerarquia

```bash
docker compose exec debian-dev bash -c '
echo "=== Jerarquia de herramientas APT ==="
echo ""
echo "Nivel BAJO:  dpkg"
echo "  Instala/desinstala .deb individuales"
echo "  NO resuelve dependencias"
echo ""
echo "Nivel MEDIO: apt-get + apt-cache + apt-mark"
echo "  apt-get:   instala, actualiza, elimina (resuelve dependencias)"
echo "  apt-cache: busca, consulta info de paquetes"
echo "  apt-mark:  marca paquetes (hold, manual, auto)"
echo ""
echo "Nivel ALTO:  apt"
echo "  Interfaz unificada que combina apt-get + apt-cache"
echo "  Mejor UX: barras de progreso, colores, formato legible"
echo ""
echo "=== Verificar que todas existen ==="
which dpkg apt-get apt-cache apt-mark apt | while read cmd; do
    echo "  $(basename $cmd): $(dpkg -S $cmd 2>/dev/null | cut -d: -f1)"
done
'
```

### Paso 1.2: dpkg es la base

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg: la base de todo ==="
echo ""
echo "Cuando ejecutas: apt install nginx"
echo "  1. apt resuelve dependencias"
echo "  2. apt descarga los .deb de los repositorios"
echo "  3. apt llama a dpkg para instalar cada .deb"
echo ""
echo "dpkg es la herramienta que realmente instala:"
dpkg --version | head -1
echo ""
echo "Paquetes instalados en este sistema:"
dpkg -l | grep "^ii" | wc -l
'
```

### Paso 1.3: Que paquete provee cada herramienta

```bash
docker compose exec debian-dev bash -c '
echo "=== Paquetes que proveen las herramientas ==="
echo ""
for cmd in /usr/bin/dpkg /usr/bin/apt-get /usr/bin/apt-cache /usr/bin/apt-mark /usr/bin/apt; do
    if [ -f "$cmd" ]; then
        pkg=$(dpkg -S "$cmd" 2>/dev/null | head -1 | cut -d: -f1)
        printf "  %-20s → paquete: %s\n" "$(basename $cmd)" "$pkg"
    fi
done
echo ""
echo "apt-get, apt-cache y apt vienen del paquete apt"
echo "dpkg viene del paquete dpkg"
echo "Son herramientas diferentes del mismo ecosistema"
'
```

---

## Parte 2 — apt vs apt-get en la practica

### Objetivo

Comparar la salida de los mismos comandos entre apt y apt-get.

### Paso 2.1: Buscar un paquete

Prediccion: ambos devuelven los mismos resultados, pero apt
muestra el output con mejor formato.

```bash
docker compose exec debian-dev bash -c '
echo "=== Buscar con apt-cache search ==="
apt-cache search "^jq$" 2>/dev/null || echo "(sin resultados)"
echo ""
echo "=== Buscar con apt search ==="
apt search "^jq$" 2>/dev/null || echo "(sin resultados)"
echo ""
echo "--- Observar ---"
echo "apt-cache search: una linea por resultado, sin formato"
echo "apt search: agrupado, con version y descripcion separada"
'
```

### Paso 2.2: Informacion de un paquete

```bash
docker compose exec debian-dev bash -c '
echo "=== apt-cache show bash ==="
apt-cache show bash 2>/dev/null | head -15
echo "..."
echo ""
echo "=== apt show bash ==="
apt show bash 2>/dev/null | head -15
echo "..."
echo ""
echo "--- Diferencia ---"
echo "apt show: output mas limpio, headers resaltados"
echo "apt-cache show: output raw, mejor para parsear en scripts"
'
```

### Paso 2.3: Listar paquetes instalados

```bash
docker compose exec debian-dev bash -c '
echo "=== Comparar listados ==="
echo ""
echo "--- apt list --installed (primeros 5) ---"
apt list --installed 2>/dev/null | head -6
echo ""
echo "--- dpkg --get-selections (primeros 5) ---"
dpkg --get-selections | head -5
echo ""
echo "--- dpkg -l (primeros 5) ---"
dpkg -l | grep "^ii" | head -5
echo ""
echo "apt list: nombre/repo version arch [estado]"
echo "dpkg --get-selections: nombre\tinstall"
echo "dpkg -l: prefijo ii + nombre + version + arch + descripcion"
'
```

### Paso 2.4: La diferencia clave — API estable

```bash
docker compose exec debian-dev bash -c '
echo "=== API estable: apt-get vs apt ==="
echo ""
echo "apt puede mostrar warnings como:"
echo "  WARNING: apt does not have a stable CLI interface."
echo "  Use with caution in scripts."
echo ""
echo "Ejemplo: redirigir output"
echo ""
echo "--- apt-get (estable, parseble) ---"
apt-get -qq --print-uris install curl 2>/dev/null | head -3
echo ""
echo "--- Regla ---"
echo "Terminal interactiva → apt"
echo "Scripts/Dockerfiles  → apt-get"
echo "Buscar/consultar     → apt-cache (o apt show)"
'
```

---

## Parte 3 — apt-cache, apt-mark y comandos exclusivos

### Objetivo

Usar apt-cache para consultas avanzadas, apt-mark para gestionar
paquetes, y conocer los comandos exclusivos de cada herramienta.

### Paso 3.1: apt-cache depends y rdepends

```bash
docker compose exec debian-dev bash -c '
echo "=== apt-cache depends: que necesita un paquete ==="
apt-cache depends bash 2>/dev/null | head -10
echo ""
echo "=== apt-cache rdepends: quien depende de este paquete ==="
apt-cache rdepends bash 2>/dev/null | head -10
echo ""
echo "depends = dependencias directas"
echo "rdepends = dependencias inversas (quien lo usa)"
'
```

### Paso 3.2: apt-cache policy

```bash
docker compose exec debian-dev bash -c '
echo "=== apt-cache policy: versiones y prioridades ==="
apt-cache policy bash
echo ""
echo "--- Campos ---"
echo "Installed: version actualmente instalada"
echo "Candidate: version que se instalaria con apt upgrade"
echo "Version table:"
echo "  *** = version instalada"
echo "  500 = prioridad del repo (default para repos habilitados)"
echo "  100 = prioridad para /var/lib/dpkg/status (local)"
'
```

### Paso 3.3: apt-mark manual y auto

```bash
docker compose exec debian-dev bash -c '
echo "=== Paquetes manuales vs automaticos ==="
echo ""
echo "Manual: el usuario los pidio explicitamente"
echo "  Cantidad: $(apt-mark showmanual | wc -l)"
echo ""
echo "Automatico: instalados como dependencia"
echo "  Cantidad: $(apt-mark showauto | wc -l)"
echo ""
echo "--- Primeros 10 manuales ---"
apt-mark showmanual | head -10
echo ""
echo "Si un paquete auto ya no es dependencia de nadie:"
echo "  apt autoremove lo eliminaria"
echo "  apt-mark manual <pkg> lo protege"
'
```

### Paso 3.4: apt-mark hold

```bash
docker compose exec debian-dev bash -c '
echo "=== Hold: evitar que un paquete se actualice ==="
echo ""
echo "Paquetes en hold actualmente:"
held=$(apt-mark showhold)
if [ -z "$held" ]; then
    echo "  (ninguno)"
else
    echo "$held"
fi
echo ""
echo "Comandos de hold:"
echo "  sudo apt-mark hold <pkg>     ← prevenir upgrade"
echo "  sudo apt-mark unhold <pkg>   ← permitir upgrade"
echo "  apt-mark showhold            ← ver paquetes en hold"
echo ""
echo "Caso de uso: evitar que el kernel se actualice"
echo "  antes de verificar compatibilidad con drivers"
'
```

### Paso 3.5: Comandos exclusivos

```bash
docker compose exec debian-dev bash -c '
echo "=== Comandos que SOLO existen en apt ==="
echo ""
echo "apt list --upgradable    → paquetes con upgrade disponible"
echo "apt list --installed     → paquetes instalados"
echo "apt edit-sources         → editar sources.list"
echo ""
echo "=== Comandos que SOLO existen en apt-get ==="
echo ""
echo "apt-get source <pkg>    → descargar codigo fuente"
echo "apt-get build-dep <pkg> → instalar deps de compilacion"
echo "apt-get download <pkg>  → descargar .deb sin instalar"
echo "apt-get clean           → limpiar cache de .deb"
echo "apt-get autoclean       → limpiar versiones viejas"
echo ""
echo "=== Cache de paquetes ==="
echo "Ubicacion: /var/cache/apt/archives/"
echo "Tamano:    $(du -sh /var/cache/apt/archives/ 2>/dev/null | cut -f1)"
ls /var/cache/apt/archives/*.deb 2>/dev/null | wc -l
echo "archivos .deb en cache"
'
```

### Paso 3.6: Tabla resumen

```bash
docker compose exec debian-dev bash -c '
echo "=== Cuando usar cada herramienta ==="
printf "%-25s %-15s %-15s\n" "Situacion" "Herramienta" "Razon"
printf "%-25s %-15s %-15s\n" "------------------------" "--------------" "--------------"
printf "%-25s %-15s %-15s\n" "Terminal interactiva" "apt" "UX, colores"
printf "%-25s %-15s %-15s\n" "Scripts de shell" "apt-get" "API estable"
printf "%-25s %-15s %-15s\n" "Dockerfiles" "apt-get" "No interactivo"
printf "%-25s %-15s %-15s\n" "Buscar paquetes" "apt/apt-cache" "Ambos ok"
printf "%-25s %-15s %-15s\n" "Consultar dependencias" "apt-cache" "depends/rdepends"
printf "%-25s %-15s %-15s\n" "Hold/unhold" "apt-mark" "Unico que lo hace"
printf "%-25s %-15s %-15s\n" "Descargar .deb" "apt-get" "download"
printf "%-25s %-15s %-15s\n" "Compilar desde fuente" "apt-get" "build-dep/source"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. APT es una **familia de herramientas**: dpkg (bajo nivel),
   apt-get/apt-cache/apt-mark (nivel medio), y apt (alto nivel
   con mejor UX).

2. `apt` y `apt-get` usan las mismas bibliotecas internamente.
   La diferencia es la **interfaz**: apt tiene colores, barra de
   progreso, y output legible; apt-get tiene output estable y
   parseble.

3. En **scripts y Dockerfiles** usar `apt-get`; en la **terminal
   interactiva** usar `apt`. apt no garantiza API estable.

4. `apt-cache policy` muestra de que repositorio viene un
   paquete y con que prioridad. `apt-cache depends/rdepends`
   muestra las relaciones de dependencia.

5. `apt-mark` gestiona el estado de los paquetes: `hold` evita
   upgrades, `manual/auto` controla que elimina `autoremove`.

6. `apt-get` tiene comandos exclusivos que apt no tiene:
   `source`, `build-dep`, `download`, `clean`, `autoclean`.
