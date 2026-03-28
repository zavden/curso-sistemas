# Lab — Repositorios

## Objetivo

Explorar la configuracion de repositorios APT: el formato one-line
y DEB822, archivos en sources.list.d, la autenticacion GPG (legacy
vs Signed-By), y como apt-cache policy decide que version instalar.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Configuracion de repositorios

### Objetivo

Entender los archivos de configuracion de APT y sus formatos.

### Paso 1.1: sources.list

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/apt/sources.list ==="
if [ -f /etc/apt/sources.list ]; then
    cat /etc/apt/sources.list
else
    echo "(no existe — puede estar usando DEB822)"
fi
echo ""
echo "=== Archivos en sources.list.d/ ==="
ls -la /etc/apt/sources.list.d/ 2>/dev/null || echo "(directorio vacio)"
'
```

### Paso 1.2: Formato one-line

```bash
docker compose exec debian-dev bash -c '
echo "=== Formato one-line (clasico) ==="
echo ""
echo "Estructura:"
echo "  type  URI                          suite      components"
echo "  deb   http://deb.debian.org/debian bookworm   main contrib non-free-firmware"
echo ""
echo "--- Campos ---"
echo "type:       deb (binarios) o deb-src (fuentes)"
echo "URI:        URL del repositorio"
echo "suite:      nombre de la release (bookworm, bookworm-security)"
echo "components: secciones del repositorio"
echo ""
echo "=== Componentes de Debian ==="
printf "%-20s %-15s %s\n" "Componente" "Licencia" "Soporte"
printf "%-20s %-15s %s\n" "-------------------" "--------------" "-------------------"
printf "%-20s %-15s %s\n" "main" "100% libre" "Soportado por Debian"
printf "%-20s %-15s %s\n" "contrib" "Libre (dep nf)" "Soportado por Debian"
printf "%-20s %-15s %s\n" "non-free" "No libre" "No soportado"
printf "%-20s %-15s %s\n" "non-free-firmware" "Firmware" "Separado (Debian 12+)"
'
```

### Paso 1.3: Formato DEB822

```bash
docker compose exec debian-dev bash -c '
echo "=== Formato DEB822 (moderno, Debian 12+) ==="
echo ""
echo "Archivos .sources en /etc/apt/sources.list.d/:"
for f in /etc/apt/sources.list.d/*.sources; do
    if [ -f "$f" ]; then
        echo ""
        echo "--- $(basename $f) ---"
        cat "$f"
    fi
done
if [ ! -f /etc/apt/sources.list.d/*.sources ] 2>/dev/null; then
    echo "(no hay archivos .sources — usando formato one-line)"
    echo ""
    echo "Ejemplo de DEB822:"
    echo "  Types: deb deb-src"
    echo "  URIs: http://deb.debian.org/debian"
    echo "  Suites: bookworm bookworm-updates"
    echo "  Components: main contrib non-free-firmware"
    echo "  Signed-By: /usr/share/keyrings/debian-archive-keyring.gpg"
fi
echo ""
echo "Ventaja de DEB822: Signed-By vincula la clave GPG al repo"
'
```

### Paso 1.4: Repos de terceros en sources.list.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos individuales en sources.list.d/ ==="
echo ""
echo "Cada repo de terceros se agrega como archivo individual:"
echo "  /etc/apt/sources.list.d/docker.list"
echo "  /etc/apt/sources.list.d/nodesource.list"
echo ""
echo "Ventaja: agregar/eliminar un repo = agregar/eliminar un archivo"
echo "No hay que editar sources.list directamente"
echo ""
echo "=== Contenido actual ==="
ls -la /etc/apt/sources.list.d/ 2>/dev/null
echo ""
echo "Archivos .list (one-line): $(ls /etc/apt/sources.list.d/*.list 2>/dev/null | wc -l)"
echo "Archivos .sources (DEB822): $(ls /etc/apt/sources.list.d/*.sources 2>/dev/null | wc -l)"
'
```

---

## Parte 2 — GPG y autenticacion

### Objetivo

Entender como APT verifica la autenticidad de los paquetes.

### Paso 2.1: Claves GPG instaladas

```bash
docker compose exec debian-dev bash -c '
echo "=== Claves GPG del sistema ==="
echo ""
echo "--- /usr/share/keyrings/ (moderno) ---"
ls /usr/share/keyrings/ 2>/dev/null || echo "(vacio)"
echo ""
echo "--- /etc/apt/keyrings/ (alternativa) ---"
ls /etc/apt/keyrings/ 2>/dev/null || echo "(vacio o no existe)"
echo ""
echo "--- /etc/apt/trusted.gpg.d/ (legacy, global) ---"
ls /etc/apt/trusted.gpg.d/ 2>/dev/null || echo "(vacio)"
'
```

### Paso 2.2: Legacy vs moderno

```bash
docker compose exec debian-dev bash -c '
echo "=== Metodo legacy: apt-key (deprecated) ==="
echo ""
echo "Problema del metodo legacy:"
echo "  apt-key add descarga.gpg"
echo "  → La clave se agrega a un keyring GLOBAL"
echo "  → CUALQUIER repo puede usar CUALQUIER clave"
echo "  → Un repo comprometido podria firmar paquetes de otro repo"
echo ""
echo "apt-key esta deprecated desde Debian 11"
echo ""
echo "=== Metodo moderno: Signed-By ==="
echo ""
echo "Cada repositorio tiene su propia clave:"
echo "  1. Clave en /usr/share/keyrings/docker.gpg"
echo "  2. Repo con: [signed-by=/usr/share/keyrings/docker.gpg]"
echo "  → Solo ESA clave puede autenticar ESE repo"
echo ""
echo "Procedimiento para agregar un repo de terceros:"
echo "  curl -fsSL https://example.com/gpg | gpg --dearmor -o /usr/share/keyrings/example.gpg"
echo "  echo \"deb [signed-by=/usr/share/keyrings/example.gpg] https://... suite main\" > /etc/apt/sources.list.d/example.list"
'
```

### Paso 2.3: Verificar autenticacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar que repos estan autenticados ==="
echo ""
echo "apt update muestra el estado de cada repo:"
echo "  Hit:  = indice sin cambios (autenticado)"
echo "  Get:  = indice descargado (autenticado)"
echo "  Ign:  = ignorado"
echo "  Err:  = error de descarga"
echo ""
echo "Si falta la clave GPG:"
echo "  W: GPG error: ... NO_PUBKEY AABBCCDD"
echo "  → Descargar e instalar la clave del proveedor"
echo ""
echo "=== Claves de la distribucion base ==="
apt-key list 2>/dev/null | head -20 || echo "(apt-key no disponible o sin claves legacy)"
'
```

---

## Parte 3 — Policy y prioridades

### Objetivo

Entender como APT decide que version de un paquete instalar.

### Paso 3.1: apt-cache policy basico

```bash
docker compose exec debian-dev bash -c '
echo "=== apt-cache policy (sin argumentos) ==="
echo "Muestra todos los repos y sus prioridades:"
echo ""
apt-cache policy | head -25
echo "..."
echo ""
echo "Prioridad 500 = default para repos habilitados"
echo "Prioridad 100 = /var/lib/dpkg/status (paquetes locales)"
'
```

### Paso 3.2: Policy de un paquete

```bash
docker compose exec debian-dev bash -c '
echo "=== apt-cache policy de paquetes especificos ==="
echo ""
echo "--- bash ---"
apt-cache policy bash
echo ""
echo "--- curl ---"
apt-cache policy curl
echo ""
echo "--- Campos ---"
echo "Installed: version instalada (none = no instalado)"
echo "Candidate: version que apt instalaria"
echo "***:       marca la version instalada"
echo "500:       prioridad del repositorio"
'
```

### Paso 3.3: Prioridades y pinning

```bash
docker compose exec debian-dev bash -c '
echo "=== Prioridades de APT ==="
printf "%-12s %s\n" "Prioridad" "Comportamiento"
printf "%-12s %s\n" "-----------" "----------------------------------------"
printf "%-12s %s\n" "> 1000" "Se instala incluso si es downgrade"
printf "%-12s %s\n" "990" "Se instala aunque no sea la mas nueva"
printf "%-12s %s\n" "500" "Default para repos habilitados"
printf "%-12s %s\n" "100" "Solo si no hay otra version (not-automatic)"
printf "%-12s %s\n" "< 0" "NUNCA se instala"
echo ""
echo "=== Pinning: controlar prioridades ==="
echo ""
echo "Archivo: /etc/apt/preferences.d/nombre"
echo ""
echo "Ejemplo — bloquear un paquete:"
echo "  Package: snapd"
echo "  Pin: release *"
echo "  Pin-Priority: -1"
echo ""
echo "Ejemplo — preferir backports:"
echo "  Package: nginx"
echo "  Pin: release a=bookworm-backports"
echo "  Pin-Priority: 600"
echo ""
echo "=== Preferencias actuales ==="
ls /etc/apt/preferences.d/ 2>/dev/null || echo "(ninguna configurada)"
cat /etc/apt/preferences.d/* 2>/dev/null || echo "(sin archivos de preferencias)"
'
```

### Paso 3.4: Backports

```bash
docker compose exec debian-dev bash -c '
echo "=== Backports ==="
echo ""
echo "Versiones mas nuevas adaptadas para la release estable"
echo ""
echo "Habilitar:"
echo "  echo \"deb http://deb.debian.org/debian bookworm-backports main\" \\"
echo "    > /etc/apt/sources.list.d/backports.list"
echo ""
echo "Prioridad: 100 (no se instalan automaticamente)"
echo "Instalar explicitamente:"
echo "  apt install -t bookworm-backports nginx"
echo ""
echo "=== Repos con backports en este sistema ==="
grep -r backports /etc/apt/sources.list /etc/apt/sources.list.d/ 2>/dev/null || \
    echo "(backports no habilitados)"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Los repositorios se configuran en `/etc/apt/sources.list` o
   como archivos individuales en `/etc/apt/sources.list.d/`.
   El formato moderno **DEB822** (.sources) reemplaza al one-line.

2. Cada linea de repositorio tiene: **type** (deb/deb-src),
   **URI**, **suite** (release), y **components** (main,
   contrib, non-free).

3. El metodo moderno de GPG usa **Signed-By**: cada repo tiene
   su propia clave en `/usr/share/keyrings/`. El metodo legacy
   `apt-key` esta deprecated porque usaba un keyring global.

4. `apt-cache policy <pkg>` muestra la **version instalada**,
   la **candidata**, y de **que repositorio** viene cada version
   con su **prioridad**.

5. **Pinning** en `/etc/apt/preferences.d/` permite cambiar las
   prioridades: bloquear paquetes (prioridad -1), preferir un
   repo sobre otro, o forzar backports.

6. Los **backports** tienen prioridad 100 por defecto — no se
   instalan automaticamente, hay que pedirlos con `-t`.
