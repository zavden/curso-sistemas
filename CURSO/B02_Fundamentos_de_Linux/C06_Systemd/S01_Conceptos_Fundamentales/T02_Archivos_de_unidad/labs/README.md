# Lab — Archivos de unidad

## Objetivo

Dominar la estructura y ubicacion de los archivos de unidad de
systemd: las tres ubicaciones con su orden de precedencia, la
anatomia de las secciones [Unit], [Service] e [Install], drop-ins
para overrides parciales, y herramientas de verificacion como
systemd-analyze verify.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Ubicaciones y precedencia

### Objetivo

Explorar los tres directorios donde residen los archivos de unidad
y entender su orden de precedencia.

### Paso 1.1: Los tres directorios

```bash
docker compose exec debian-dev bash -c '
echo "=== Ubicaciones de archivos de unidad ==="
echo ""
echo "--- /usr/lib/systemd/system/ (paquetes, precedencia MINIMA) ---"
PKGS=$(ls /usr/lib/systemd/system/ 2>/dev/null | wc -l)
echo "Total de archivos: $PKGS"
ls /usr/lib/systemd/system/*.service 2>/dev/null | head -5
echo "..."
echo ""
echo "--- /etc/systemd/system/ (admin, precedencia MAXIMA) ---"
ADMIN=$(ls /etc/systemd/system/ 2>/dev/null | wc -l)
echo "Total de archivos: $ADMIN"
ls /etc/systemd/system/ 2>/dev/null | head -10
echo ""
echo "--- /run/systemd/system/ (runtime, precedencia INTERMEDIA) ---"
RUN=$(ls /run/systemd/system/ 2>/dev/null | wc -l)
echo "Total de archivos: $RUN"
ls /run/systemd/system/ 2>/dev/null | head -5
'
```

### Paso 1.2: Precedencia en accion

Antes de continuar, predice: si existe el mismo archivo en
`/usr/lib/systemd/system/` y `/etc/systemd/system/`, cual
se usa?

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden de precedencia ==="
echo ""
echo "/etc/systemd/system/       ← MAXIMA (admin)"
echo "      ↓ sobrescribe"
echo "/run/systemd/system/       ← intermedia (runtime)"
echo "      ↓ sobrescribe"
echo "/usr/lib/systemd/system/   ← MINIMA (paquetes)"
echo ""
echo "El admin SIEMPRE gana sobre el paquete"
echo ""
echo "--- Verificar: que hay en /etc/ que override /usr/lib/ ---"
for f in /etc/systemd/system/*.service 2>/dev/null; do
    [[ -f "$f" ]] || continue
    BASE=$(basename "$f")
    if [[ -f "/usr/lib/systemd/system/$BASE" ]]; then
        echo "  OVERRIDE: $BASE"
        echo "    /etc/ sobrescribe /usr/lib/"
    fi
done
echo ""
echo "Si no hay overrides, el sistema usa los archivos de paquete"
'
```

### Paso 1.3: Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian ==="
echo ""
echo "--- /lib/systemd/system/ (puede ser symlink a /usr/lib/) ---"
if [[ -L /lib/systemd/system ]]; then
    echo "/lib/systemd/system → $(readlink /lib/systemd/system)"
    echo "(sistemas con /usr merge: /lib es symlink)"
elif [[ -d /lib/systemd/system ]]; then
    echo "/lib/systemd/system es directorio real"
    ls /lib/systemd/system/*.service 2>/dev/null | wc -l
    echo "archivos .service"
fi
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux (RHEL) ==="
echo ""
echo "--- /usr/lib/systemd/system/ ---"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo "archivos .service en /usr/lib/systemd/system/"
echo ""
echo "RHEL siempre usa /usr/lib/systemd/system/"
echo "(no usa /lib/ como alternativa)"
'
```

---

## Parte 2 — Estructura interna

### Objetivo

Analizar las secciones y directivas de un archivo de unidad real.

### Paso 2.1: Leer un servicio completo

```bash
docker compose exec debian-dev bash -c '
echo "=== Leer un servicio ==="
echo ""
SVC=$(ls /usr/lib/systemd/system/systemd-journald.service \
         /lib/systemd/system/cron.service 2>/dev/null | head -1)
if [[ -n "$SVC" ]]; then
    echo "Archivo: $SVC"
    echo ""
    cat -n "$SVC"
    echo ""
    echo "--- Analisis ---"
    echo "Secciones encontradas:"
    grep -n "^\[" "$SVC" | sed "s/^/  /"
fi
'
```

### Paso 2.2: Seccion [Unit]

```bash
docker compose exec debian-dev bash -c '
echo "=== Seccion [Unit] ==="
echo ""
echo "Directivas comunes en [Unit]:"
echo ""
echo "  Description=      Descripcion legible del servicio"
echo "  Documentation=    URLs o man pages"
echo "  After=            Arrancar DESPUES de estas unidades"
echo "  Before=           Arrancar ANTES de estas unidades"
echo "  Wants=            Dependencia debil (intenta arrancar)"
echo "  Requires=         Dependencia fuerte (falla si no arranca)"
echo "  Conflicts=        No puede coexistir con estas unidades"
echo "  ConditionPathExists=  Solo arranca si la ruta existe"
echo ""
echo "--- Ejemplo real ---"
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    AFTER=$(grep "^After=" "$svc" 2>/dev/null)
    if [[ -n "$AFTER" ]]; then
        echo "  $(basename "$svc"):"
        echo "    $AFTER"
        break
    fi
done
'
```

### Paso 2.3: Seccion [Service]

```bash
docker compose exec debian-dev bash -c '
echo "=== Seccion [Service] ==="
echo ""
echo "Directivas comunes en [Service]:"
echo ""
echo "  Type=           Como systemd detecta que arranco"
echo "                  simple (default), forking, notify, oneshot, exec"
echo ""
echo "  ExecStart=      Comando principal (ruta ABSOLUTA)"
echo "  ExecStartPre=   Antes de arrancar (puede ser multiple)"
echo "  ExecReload=     Al hacer reload"
echo "  ExecStop=       Al hacer stop (default: SIGTERM)"
echo ""
echo "  User= Group=    Usuario y grupo del proceso"
echo "  WorkingDirectory= Directorio de trabajo"
echo "  Environment=    Variables de entorno"
echo "  EnvironmentFile= Variables desde archivo"
echo ""
echo "  Restart=        Cuando reiniciar (on-failure, always, no)"
echo "  RestartSec=     Segundos entre reinicios"
echo ""
echo "--- Tipos de servicio en el sistema ---"
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    TYPE=$(grep "^Type=" "$svc" 2>/dev/null | head -1)
    if [[ -n "$TYPE" ]]; then
        printf "  %-45s %s\n" "$(basename "$svc")" "$TYPE"
    fi
done | head -10
'
```

### Paso 2.4: Seccion [Install]

```bash
docker compose exec debian-dev bash -c '
echo "=== Seccion [Install] ==="
echo ""
echo "Directivas en [Install]:"
echo ""
echo "  WantedBy=    Target que quiere esta unidad (al hacer enable)"
echo "  RequiredBy=  Target que requiere esta unidad"
echo "  Also=        Unidades a habilitar/deshabilitar juntas"
echo "  Alias=       Nombres alternativos"
echo ""
echo "Lo que hace systemctl enable:"
echo "  Lee WantedBy=multi-user.target"
echo "  Crea symlink en /etc/systemd/system/multi-user.target.wants/"
echo ""
echo "--- Servicios sin [Install] (static) ---"
COUNT=0
for svc in /usr/lib/systemd/system/systemd-*.service; do
    [[ -f "$svc" ]] || continue
    if ! grep -q "^\[Install\]" "$svc" 2>/dev/null; then
        echo "  $(basename "$svc") — sin [Install] (static)"
        ((COUNT++))
        [[ $COUNT -ge 5 ]] && break
    fi
done
echo ""
echo "Las unidades static no se pueden enable/disable directamente"
echo "Se activan como dependencia de otra unidad"
'
```

---

## Parte 3 — Drop-ins y verificacion

### Objetivo

Crear archivos de override (drop-in) y usar systemd-analyze verify
para validar unidades.

### Paso 3.1: Crear un unit file de ejemplo

```bash
docker compose exec debian-dev bash -c '
echo "=== Crear un unit file de ejemplo ==="
echo ""
cat > /tmp/lab-example.service << '\''EOF'\''
[Unit]
Description=Lab Example Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/sleep infinity
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
echo "Archivo creado: /tmp/lab-example.service"
echo ""
cat -n /tmp/lab-example.service
echo ""
echo "--- Verificar con systemd-analyze ---"
systemd-analyze verify /tmp/lab-example.service 2>&1 || true
echo ""
echo "Si no hay errores, la unidad es sintacticamente correcta"
'
```

### Paso 3.2: Detectar errores de sintaxis

Antes de ejecutar, predice: que errores encontrara
systemd-analyze verify en estos archivos?

```bash
docker compose exec debian-dev bash -c '
echo "=== Detectar errores ==="
echo ""
echo "--- Archivo con errores ---"
cat > /tmp/lab-bad.service << '\''EOF'\''
[Unit]
Description=Bad Service

[Service]
Type=simple
ExecStart=relative-path
Restart=maybe

[Install]
WantedBy=multi-user.target
EOF
echo "Archivo con errores:"
cat -n /tmp/lab-bad.service
echo ""
echo "--- Verificacion ---"
systemd-analyze verify /tmp/lab-bad.service 2>&1 || true
echo ""
echo "Errores tipicos que detecta:"
echo "  - ExecStart sin ruta absoluta"
echo "  - Valores invalidos para Restart="
echo "  - Secciones desconocidas"
echo "  - Directivas en la seccion incorrecta"
rm /tmp/lab-bad.service
'
```

### Paso 3.3: Drop-in overrides

```bash
docker compose exec debian-dev bash -c '
echo "=== Drop-in overrides ==="
echo ""
echo "Un drop-in modifica una unidad SIN reemplazarla"
echo "Se crea en: /etc/systemd/system/UNIDAD.d/override.conf"
echo ""
echo "--- Estructura ---"
echo "/usr/lib/systemd/system/nginx.service     ← original (paquete)"
echo "/etc/systemd/system/nginx.service.d/      ← directorio de drop-ins"
echo "  ├── 10-limits.conf                      ← override 1"
echo "  └── 20-restart.conf                     ← override 2"
echo ""
echo "--- Ejemplo de drop-in ---"
cat << '\''EOF'\''
# /etc/systemd/system/nginx.service.d/override.conf
[Service]
MemoryMax=1G
LimitNOFILE=65535
EOF
echo ""
echo "--- Buscar drop-ins existentes ---"
find /etc/systemd/system -name "*.d" -type d 2>/dev/null | head -10
echo ""
echo "Los drop-ins se aplican en orden alfabetico"
echo "Para LIMPIAR una directiva de lista antes de redefinir:"
echo "  ExecStart=                    ← limpia el valor original"
echo "  ExecStart=/new/path           ← nuevo valor"
echo ""
echo "Despues de crear/editar drop-ins:"
echo "  systemctl daemon-reload       ← re-leer archivos"
echo "  systemctl restart servicio    ← aplicar cambios"
'
```

### Paso 3.4: Comparar Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian: ubicaciones ==="
echo ""
echo "Servicios en /usr/lib/systemd/system/:"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo ""
echo "Servicios en /etc/systemd/system/:"
ls /etc/systemd/system/*.service 2>/dev/null | wc -l
'
```

```bash
docker compose exec alma-dev bash -c '
echo "=== AlmaLinux: ubicaciones ==="
echo ""
echo "Servicios en /usr/lib/systemd/system/:"
ls /usr/lib/systemd/system/*.service 2>/dev/null | wc -l
echo ""
echo "Servicios en /etc/systemd/system/:"
ls /etc/systemd/system/*.service 2>/dev/null | wc -l
echo ""
echo "--- Comparar un servicio ---"
if [[ -f /usr/lib/systemd/system/crond.service ]]; then
    echo "RHEL usa crond.service (con d):"
    cat /usr/lib/systemd/system/crond.service
fi
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/lab-example.service /tmp/lab-bad.service
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. Los archivos de unidad se buscan en tres ubicaciones con
   precedencia: `/etc/systemd/system/` (admin, maxima) >
   `/run/systemd/system/` (runtime) >
   `/usr/lib/systemd/system/` (paquetes, minima).

2. Cada archivo de unidad usa formato INI con tres secciones
   principales: `[Unit]` (metadata y dependencias), `[Service]`
   (configuracion del proceso), `[Install]` (integracion al boot).

3. `ExecStart=` requiere una ruta absoluta. `Type=` define como
   systemd detecta que el servicio arranco (simple, forking,
   notify, oneshot).

4. Las unidades sin seccion `[Install]` son "static": no se
   pueden enable/disable directamente, solo se activan como
   dependencia de otra unidad.

5. Los drop-ins en `.d/override.conf` permiten modificar una
   unidad sin reemplazarla. Se aplican en orden alfabetico.
   Para limpiar una directiva de lista, asignar vacio primero.

6. `systemd-analyze verify` valida la sintaxis de un archivo de
   unidad sin necesidad de cargarlo en systemd. Detecta rutas
   relativas, valores invalidos y directivas mal ubicadas.
