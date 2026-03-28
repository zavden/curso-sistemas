# Lab — dpkg

## Objetivo

Dominar dpkg como herramienta de bajo nivel: consultar paquetes
instalados, buscar que paquete instalo un archivo, inspeccionar
y extraer un .deb sin instalar, y verificar la integridad de los
archivos de un paquete.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Consultar paquetes instalados

### Objetivo

Usar los flags de consulta de dpkg para explorar los paquetes
del sistema.

### Paso 1.1: Listar paquetes (dpkg -l)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -l: listar paquetes ==="
dpkg -l | head -8
echo "..."
echo ""
echo "--- Prefijos (dos letras) ---"
echo "Primera letra (desired): i=install r=remove p=purge h=hold"
echo "Segunda letra (status):  i=installed c=config-files n=not-installed"
echo ""
echo "Combinaciones comunes:"
echo "  ii = correctamente instalado"
echo "  rc = removido pero config presente"
echo "  pn = purgado"
echo "  hi = en hold, instalado"
echo ""
echo "Total paquetes instalados (ii):"
dpkg -l | grep "^ii" | wc -l
'
```

### Paso 1.2: Archivos de un paquete (dpkg -L)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -L: archivos instalados por un paquete ==="
echo ""
echo "--- Archivos de bash ---"
dpkg -L bash | head -15
echo "..."
echo ""
total=$(dpkg -L bash | wc -l)
echo "Total archivos instalados por bash: $total"
echo ""
echo "--- Solo binarios ---"
dpkg -L bash | grep "/bin/"
'
```

### Paso 1.3: Buscar dueno de un archivo (dpkg -S)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -S: quien instalo este archivo ==="
echo ""
echo "--- Archivos conocidos ---"
dpkg -S /usr/bin/bash
dpkg -S /usr/bin/env
dpkg -S /usr/bin/find 2>/dev/null || echo "/usr/bin/find: no encontrado"
dpkg -S /usr/bin/ls
echo ""
echo "--- Buscar con patron ---"
dpkg -S "*nginx.conf*" 2>/dev/null || echo "(nginx no instalado — normal)"
echo ""
echo "--- Archivo no gestionado por dpkg ---"
dpkg -S /usr/local/bin/algo 2>/dev/null || \
    echo "dpkg-query: no path found → fue instalado manualmente"
'
```

### Paso 1.4: Informacion detallada (dpkg -s)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -s: informacion del paquete ==="
dpkg -s bash
echo ""
echo "--- Campos importantes ---"
echo "Status:         estado de instalacion"
echo "Installed-Size: tamano en KB"
echo "Depends:        dependencias"
echo "Description:    descripcion del paquete"
'
```

---

## Parte 2 — Inspeccionar un .deb

### Objetivo

Descargar, inspeccionar, y extraer un archivo .deb sin instalarlo.

### Paso 2.1: Descargar un .deb

```bash
docker compose exec debian-dev bash -c '
echo "=== Descargar un .deb sin instalar ==="
cd /tmp
apt-get download cowsay 2>/dev/null
echo ""
echo "Archivo descargado:"
ls -lh /tmp/cowsay*.deb 2>/dev/null || echo "(no se pudo descargar — intentando con apt-get update)"
if [ ! -f /tmp/cowsay*.deb ]; then
    apt-get update -qq 2>/dev/null
    apt-get download cowsay 2>/dev/null
    ls -lh /tmp/cowsay*.deb 2>/dev/null
fi
echo ""
echo "apt-get download descarga el .deb al directorio actual"
echo "sin instalarlo (no necesita sudo)"
'
```

### Paso 2.2: Inspeccionar sin instalar (dpkg -I)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -I: informacion del .deb ==="
deb=$(ls /tmp/cowsay*.deb 2>/dev/null | head -1)
if [ -f "$deb" ]; then
    dpkg -I "$deb"
else
    echo "(no hay .deb para inspeccionar)"
    echo ""
    echo "dpkg -I muestra:"
    echo "  Package, Version, Architecture"
    echo "  Depends, Description"
    echo "  Tamano instalado"
fi
'
```

### Paso 2.3: Listar contenido (dpkg -c)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -c: listar archivos del .deb ==="
deb=$(ls /tmp/cowsay*.deb 2>/dev/null | head -1)
if [ -f "$deb" ]; then
    dpkg -c "$deb" | head -20
    echo "..."
    echo ""
    total=$(dpkg -c "$deb" | wc -l)
    echo "Total archivos en el .deb: $total"
else
    echo "(no hay .deb)"
fi
'
```

### Paso 2.4: Extraer sin instalar (dpkg -x)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -x: extraer contenido ==="
deb=$(ls /tmp/cowsay*.deb 2>/dev/null | head -1)
if [ -f "$deb" ]; then
    mkdir -p /tmp/cowsay-extracted
    dpkg -x "$deb" /tmp/cowsay-extracted
    echo "Contenido extraido:"
    find /tmp/cowsay-extracted -type f | head -15
    echo ""
    echo "Los archivos estan en /tmp/cowsay-extracted"
    echo "pero NO estan instalados (dpkg no sabe de ellos)"
else
    echo "(no hay .deb)"
fi
'
```

### Paso 2.5: Un .deb es un archive ar

```bash
docker compose exec debian-dev bash -c '
echo "=== Estructura interna de un .deb ==="
deb=$(ls /tmp/cowsay*.deb 2>/dev/null | head -1)
if [ -f "$deb" ]; then
    echo "Un .deb es un archive ar con 3 partes:"
    echo ""
    mkdir -p /tmp/deb-ar && cd /tmp/deb-ar
    ar x "$deb" 2>/dev/null
    ls -la /tmp/deb-ar/
    echo ""
    echo "debian-binary:   version del formato ($(cat /tmp/deb-ar/debian-binary 2>/dev/null))"
    echo "control.tar.*:   metadatos y scripts (preinst, postinst, etc.)"
    echo "data.tar.*:      los archivos del paquete"
    echo ""
    echo "--- Contenido de control.tar ---"
    tar -tf /tmp/deb-ar/control.tar* 2>/dev/null
    rm -rf /tmp/deb-ar
else
    echo "(no hay .deb para examinar)"
    echo ""
    echo "Un .deb es un archive ar que contiene:"
    echo "  debian-binary  → version del formato (2.0)"
    echo "  control.tar.xz → metadatos y scripts"
    echo "  data.tar.xz    → los archivos del paquete"
fi
'
```

---

## Parte 3 — Verificar y diagnosticar

### Objetivo

Usar dpkg -V para detectar archivos modificados y dpkg-query
para consultas avanzadas.

### Paso 3.1: Verificar integridad (dpkg -V)

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg -V: verificar archivos de un paquete ==="
echo ""
echo "--- Verificar bash ---"
result=$(dpkg -V bash 2>/dev/null)
if [ -z "$result" ]; then
    echo "bash: OK (sin modificaciones)"
else
    echo "$result"
fi
echo ""
echo "--- Verificar coreutils ---"
dpkg -V coreutils 2>/dev/null | head -5
echo ""
echo "Si no hay output = todos los archivos estan intactos"
echo ""
echo "--- Codigos de verificacion ---"
echo "S = tamano cambio"
echo "5 = checksum MD5 cambio (contenido modificado)"
echo "T = mtime cambio"
echo "U = user cambio"
echo "G = group cambio"
echo "M = permisos cambiaron"
echo ". = test paso OK"
echo "? = test no realizado"
echo "c = archivo de configuracion (normal que cambie)"
'
```

### Paso 3.2: Verificar archivos de configuracion

```bash
docker compose exec debian-dev bash -c '
echo "=== Archivos de configuracion modificados ==="
echo ""
echo "Es NORMAL que archivos de config (c) esten modificados"
echo "— el administrador los edito intencionalmente"
echo ""
echo "Si un BINARIO aparece modificado, es alerta de seguridad"
echo ""
echo "--- Verificar todos los paquetes (puede tardar) ---"
dpkg -V 2>/dev/null | head -15
echo "..."
echo ""
echo "Filtrar solo binarios modificados (no config):"
dpkg -V 2>/dev/null | grep -v " c " | head -10
echo "(si no hay output, ningun binario fue modificado)"
'
```

### Paso 3.3: dpkg-query avanzado

```bash
docker compose exec debian-dev bash -c '
echo "=== dpkg-query -W: formato personalizado ==="
echo ""
echo "--- Los 10 paquetes mas grandes ---"
dpkg-query -W -f "\${Installed-Size}\t\${Package}\n" 2>/dev/null | sort -rn | head -10
echo ""
echo "--- Paquetes con formato especifico ---"
echo "(nombre version tamano)"
dpkg-query -W -f "\${Package} \${Version} \${Installed-Size}KB\n" bash coreutils apt 2>/dev/null
echo ""
echo "--- Contar paquetes por estado ---"
echo "Instalados:  $(dpkg -l | grep \"^ii\" | wc -l)"
echo "Config-only: $(dpkg -l | grep \"^rc\" | wc -l)"
echo "Purgados:    $(dpkg -l | grep \"^pn\" | wc -l)"
'
```

### Paso 3.4: Base de datos de dpkg

```bash
docker compose exec debian-dev bash -c '
echo "=== Base de datos de dpkg ==="
echo ""
echo "Ubicacion: /var/lib/dpkg/"
ls /var/lib/dpkg/ | head -10
echo ""
echo "--- Archivos principales ---"
echo "status:    estado de todos los paquetes"
echo "available: paquetes disponibles"
echo "info/:     metadatos por paquete"
echo ""
echo "Tamano de la BD:"
du -sh /var/lib/dpkg/ 2>/dev/null
echo ""
echo "Metadatos de un paquete:"
ls /var/lib/dpkg/info/bash.* 2>/dev/null
echo ""
echo ".list:     archivos del paquete"
echo ".md5sums:  checksums para verificacion"
echo ".conffiles: archivos de configuracion"
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -rf /tmp/cowsay*.deb /tmp/cowsay-extracted /tmp/deb-ar 2>/dev/null
echo "Limpieza completada"
'
```

---

## Conceptos reforzados

1. `dpkg -l` lista paquetes con prefijos de dos letras: primera
   (desired: i/r/p/h) y segunda (status: i/c/n). **ii** =
   correctamente instalado, **rc** = removido con config.

2. `dpkg -L <pkg>` lista archivos de un paquete instalado.
   `dpkg -S <file>` busca que paquete instalo un archivo.
   `dpkg -s <pkg>` muestra informacion detallada.

3. Un `.deb` se puede inspeccionar sin instalar: `dpkg -I` (info),
   `dpkg -c` (contenido), `dpkg -x` (extraer). Internamente es un
   archive `ar` con `debian-binary`, `control.tar`, y `data.tar`.

4. `dpkg -V` verifica que los archivos de un paquete no fueron
   modificados. Archivos de configuracion (**c**) modificados son
   normales; binarios modificados son alerta de seguridad.

5. `dpkg-query -W -f` permite consultas con formato personalizado,
   como listar los paquetes mas grandes del sistema.

6. La base de datos de dpkg esta en `/var/lib/dpkg/`. El archivo
   `status` contiene el estado de todos los paquetes; `info/`
   contiene metadatos individuales (.list, .md5sums, .conffiles).
