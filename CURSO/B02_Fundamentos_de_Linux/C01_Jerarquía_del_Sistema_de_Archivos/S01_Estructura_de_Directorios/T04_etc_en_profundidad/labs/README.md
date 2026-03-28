# Lab — /etc en profundidad

## Objetivo

Explorar los archivos de configuracion fundamentales en /etc, entender
el patron de directorios .d/ para configuracion modular, y comparar
las diferencias entre Debian y AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Archivos fundamentales

### Objetivo

Examinar los archivos de configuracion mas importantes del sistema.

### Paso 1.1: hostname

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/hostname ==="
cat /etc/hostname

echo ""
echo "=== hostname command ==="
hostname
'
```

`/etc/hostname` contiene el nombre del host en una sola linea.
En los contenedores del curso, el hostname es el definido en el
compose.yml.

### Paso 1.2: hosts

```bash
docker compose exec debian-dev bash -c '
echo "=== /etc/hosts ==="
cat /etc/hosts
'
```

`/etc/hosts` permite resolucion de nombres sin DNS. Docker agrega
entradas automaticamente para los contenedores de la misma red.

### Paso 1.3: passwd y group

```bash
docker compose exec debian-dev bash -c '
echo "=== Usuarios (primeros y ultimos) ==="
head -3 /etc/passwd
echo "..."
tail -3 /etc/passwd

echo ""
echo "=== Formato: usuario:x:UID:GID:comentario:home:shell ==="
grep "^dev:" /etc/passwd
'
```

El campo `x` en la segunda columna indica que la contrasena esta en
`/etc/shadow` (no en passwd).

### Paso 1.4: nsswitch.conf

```bash
docker compose exec debian-dev bash -c '
echo "=== Orden de busqueda de nombres ==="
grep "^hosts:" /etc/nsswitch.conf
'
```

`hosts: files dns` significa: buscar primero en `/etc/hosts`, luego
en DNS. Este orden determina la prioridad de resolucion de nombres.

### Paso 1.5: resolv.conf

```bash
docker compose exec debian-dev bash -c '
echo "=== DNS configurado ==="
cat /etc/resolv.conf
'
```

Docker inyecta la configuracion DNS del host en los contenedores.

### Paso 1.6: os-release

```bash
docker compose exec debian-dev bash -c '
echo "=== Identificacion del sistema ==="
cat /etc/os-release
'
```

`/etc/os-release` es la forma estandar de identificar la distribucion.
Definido por systemd, presente en todas las distros modernas.

---

## Parte 2 — Patron .d/

### Objetivo

Entender la configuracion modular con directorios .d/ y su orden
de carga.

### Paso 2.1: Listar directorios .d/

```bash
docker compose exec debian-dev bash -c '
echo "=== Directorios .d/ en /etc ==="
find /etc -maxdepth 2 -name "*.d" -type d 2>/dev/null | sort | head -15
'
```

Los directorios `.d/` permiten configuracion modular: cada paquete
agrega su propio fragmento sin modificar el archivo principal.

### Paso 2.2: Ejemplo: /etc/apt/sources.list.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== sources.list.d ==="
ls /etc/apt/sources.list.d/ 2>/dev/null || echo "(vacio)"
echo ""
echo "=== sources.list principal ==="
cat /etc/apt/sources.list 2>/dev/null | grep -v "^#" | grep -v "^$" || echo "(no existe o vacio)"
'
```

### Paso 2.3: Ejemplo: /etc/sysctl.d/

```bash
docker compose exec debian-dev bash -c '
echo "=== sysctl.d ==="
ls /etc/sysctl.d/ 2>/dev/null || echo "(vacio)"
echo ""
echo "=== sysctl.conf ==="
cat /etc/sysctl.conf 2>/dev/null | grep -v "^#" | grep -v "^$" | head -5 || echo "(vacio)"
'
```

Los archivos en `.d/` se procesan en orden alfabetico. Los prefijos
numericos controlan la prioridad: `10-defaults.conf` se carga antes
que `99-override.conf`.

### Paso 2.4: Ventaja del patron .d/

```bash
docker compose exec debian-dev bash -c '
echo "=== profile.d ==="
ls /etc/profile.d/ 2>/dev/null
'
```

Cada paquete puede agregar un script en `/etc/profile.d/` sin tocar
`/etc/profile`. Esto evita conflictos en actualizaciones: el gestor
de paquetes actualiza el archivo principal sin perder configuraciones
custom del administrador.

### Paso 2.5: Contar archivos de configuracion

```bash
docker compose exec debian-dev bash -c '
echo "Archivos en /etc: $(find /etc -type f 2>/dev/null | wc -l)"
echo "Directorios .d/: $(find /etc -name "*.d" -type d 2>/dev/null | wc -l)"
'
```

---

## Parte 3 — Comparar entre distros

### Objetivo

Identificar las diferencias de configuracion entre Debian y AlmaLinux.

### Paso 3.1: Identificacion

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c '
cat /etc/debian_version
grep "^ID=" /etc/os-release
'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c '
cat /etc/redhat-release
grep "^ID=" /etc/os-release
'
```

Debian tiene `/etc/debian_version`. AlmaLinux tiene
`/etc/redhat-release`. Ambos tienen `/etc/os-release` (estandar).

### Paso 3.2: Repositorios

```bash
echo "=== Debian: /etc/apt/ ==="
docker compose exec debian-dev ls /etc/apt/ 2>/dev/null

echo ""
echo "=== AlmaLinux: /etc/yum.repos.d/ ==="
docker compose exec alma-dev ls /etc/yum.repos.d/ 2>/dev/null
```

### Paso 3.3: Locale

```bash
echo "=== Debian ==="
docker compose exec debian-dev bash -c 'cat /etc/default/locale 2>/dev/null || echo "LANG=$LANG"'

echo ""
echo "=== AlmaLinux ==="
docker compose exec alma-dev bash -c 'cat /etc/locale.conf 2>/dev/null || echo "LANG=$LANG"'
```

Debian usa `/etc/default/locale`. AlmaLinux usa `/etc/locale.conf`.

### Paso 3.4: Archivos exclusivos de cada distro

```bash
echo "=== Solo en Debian ==="
docker compose exec debian-dev bash -c '
ls /etc/debian_version /etc/dpkg/ /etc/apt/ 2>/dev/null | head -5
'

echo ""
echo "=== Solo en AlmaLinux ==="
docker compose exec alma-dev bash -c '
ls /etc/redhat-release /etc/rpm/ /etc/yum.repos.d/ 2>/dev/null | head -5
'
```

### Paso 3.5: Archivos comunes

```bash
echo "=== Archivos comunes ==="
docker compose exec debian-dev bash -c '
for f in /etc/hostname /etc/hosts /etc/passwd /etc/group /etc/resolv.conf /etc/nsswitch.conf /etc/os-release; do
    [ -f "$f" ] && echo "$f: existe" || echo "$f: NO existe"
done
'
```

Los archivos fundamentales del FHS existen en ambas distribuciones.

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. `/etc` contiene la configuracion **unica por maquina**. Dos
   servidores con el mismo software pero diferente `/etc` se
   comportan de forma diferente.

2. El patron `.d/` permite configuracion **modular**: cada paquete
   agrega su fragmento sin modificar el archivo principal. Los
   archivos se cargan en orden alfabetico.

3. `/etc/os-release` es la forma **estandar** de identificar la
   distribucion. Debian tiene ademas `/etc/debian_version`, AlmaLinux
   tiene `/etc/redhat-release`.

4. Las rutas de configuracion especificas difieren: Debian usa
   `/etc/apt/` y `/etc/default/locale`, AlmaLinux usa
   `/etc/yum.repos.d/` y `/etc/locale.conf`.

5. Los archivos fundamentales del FHS (`hostname`, `hosts`, `passwd`,
   `resolv.conf`, `nsswitch.conf`) existen en ambas distribuciones
   con el mismo formato.
