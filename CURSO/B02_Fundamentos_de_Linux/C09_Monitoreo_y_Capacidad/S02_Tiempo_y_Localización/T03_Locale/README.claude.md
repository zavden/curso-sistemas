# T03 — Locale

> **Objetivo:** Entender cómo Linux maneja los locales, configurar el idioma y formato regional, y evitar problemas comunes con codificaciones y ordenamiento en scripts.

## Erratas detectadas en el material original

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 65 | Salida `Tuesday 17 of March of 2026` para format `"%A %d de %B de %Y"` con `LC_TIME=en_US.UTF-8` — `de` es texto literal en el format string, no se traduce a `of` | La salida real sería `Tuesday 17 de March de 2026` (nombres en inglés, pero `de` queda literal) |
| `README.max.md` | 393 | `CAMBiar:` con mayúsculas/minúsculas mezcladas en el quick reference | Debería ser `CAMBIAR:` (todo mayúsculas, como el resto de encabezados del bloque) |

---

## Qué es un locale

Un locale define las **convenciones regionales** del sistema: idioma, formato de fechas, números, moneda, ordenamiento de texto y codificación de caracteres.

```
┌─────────────────────────────────────────────────────────────────────────┐
│  LOCALE = idioma + territorio + codificación                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  idioma_TERRITORIO.CODIFICACIÓN                                       │
│                                                                       │
│  es_ES.UTF-8   → Español de España, UTF-8                             │
│  en_US.UTF-8   → Inglés de Estados Unidos, UTF-8                      │
│  pt_BR.UTF-8   → Portugués de Brasil, UTF-8                           │
│  de_DE.UTF-8   → Alemán de Alemania, UTF-8                            │
│                                                                       │
│  El TERRITORIO importa:                                               │
│  es_ES → fecha: 17/03/2026, moneda: €                                │
│  es_MX → fecha: 17/03/2026, moneda: $                                │
│  en_US → fecha: 03/17/2026, moneda: $                                │
│  en_GB → fecha: 17/03/2026, moneda: £                                │
└─────────────────────────────────────────────────────────────────────────┘
```

```bash
# Ver el locale actual:
locale
# LANG=en_US.UTF-8
# LC_CTYPE="en_US.UTF-8"
# LC_NUMERIC="en_US.UTF-8"
# LC_TIME="en_US.UTF-8"
# LC_COLLATE="en_US.UTF-8"
# LC_MONETARY="en_US.UTF-8"
# LC_MESSAGES="en_US.UTF-8"
# LC_PAPER="en_US.UTF-8"
# LC_NAME="en_US.UTF-8"
# LC_ADDRESS="en_US.UTF-8"
# LC_TELEPHONE="en_US.UTF-8"
# LC_MEASUREMENT="en_US.UTF-8"
# LC_IDENTIFICATION="en_US.UTF-8"
# LC_ALL=
```

---

## Variables LC_*

Cada aspecto del locale se controla con una variable diferente:

| Variable | Afecta | Ejemplo |
|----------|--------|---------|
| `LANG` | Default para todas las LC_* no definidas | Catch-all |
| `LC_CTYPE` | Clasificación de caracteres (isalpha, mayúsculas/minúsculas) | Afecta regex, `tr`, `wc` |
| `LC_NUMERIC` | Formato de números | Separador decimal `.` vs `,` |
| `LC_TIME` | Formato de fechas y horas | `17/03/2026` vs `03/17/2026` |
| `LC_COLLATE` | Ordenamiento de texto | `Á` después de `A` o de `Z` |
| `LC_MONETARY` | Formato de moneda | `$100.00` vs `100,00 €` |
| `LC_MESSAGES` | Idioma de mensajes del sistema | "File not found" vs "Archivo no encontrado" |
| `LC_PAPER` | Tamaño de papel | A4 vs Letter |
| `LC_NAME` | Formato de nombres de personas | |
| `LC_ADDRESS` | Formato de direcciones | |
| `LC_TELEPHONE` | Formato de teléfonos | |
| `LC_MEASUREMENT` | Sistema de medidas | Métrico vs imperial |
| `LC_ALL` | **Override total** — ignora todas las anteriores | Solo para forzar en contextos específicos |

### Ejemplos prácticos

```bash
# LC_TIME — formato de fechas:
LC_TIME=es_ES.UTF-8 date +"%A %d de %B de %Y"
# martes 17 de marzo de 2026

LC_TIME=en_US.UTF-8 date +"%A %B %d, %Y"
# Tuesday March 17, 2026

# LC_NUMERIC — separador decimal:
echo "1234567.89" | LC_NUMERIC=de_DE.UTF-8 numfmt --grouping
# 1.234.567,89

# LC_MESSAGES — idioma de errores:
LC_MESSAGES=es_ES.UTF-8 ls /noexiste 2>&1
# ls: no se puede acceder a '/noexiste': No existe el archivo o el directorio

LC_MESSAGES=en_US.UTF-8 ls /noexiste 2>&1
# ls: cannot access '/noexiste': No such file or directory

# LC_COLLATE — ordenamiento:
echo -e "banana\nÁrbol\napple" | LC_ALL=C sort
# apple
# banana
# Árbol         ← Á después de Z (valor byte)

echo -e "banana\nÁrbol\napple" | LC_ALL=es_ES.UTF-8 sort
# apple
# Árbol         ← Á con la A (lingüísticamente correcto)
# banana
```

### Precedencia de variables

```
┌─────────────────────────────────────────────────────────────────────────┐
│  PRECEDENCIA (de mayor a menor)                                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  1. LC_ALL        → override total (ignora TODO)                      │
│  2. LC_*          → cada variable individual                          │
│  3. LANG          → valor por defecto                                 │
│                                                                       │
│  Ejemplo:                                                             │
│  LANG=es_ES.UTF-8         → default: español                         │
│  LC_TIME=en_US.UTF-8      → pero fechas en inglés                    │
│  LC_ALL=                   → vacío, no sobreescribe nada              │
│                                                                       │
│  Si LC_ALL tiene valor, ignora LANG y TODAS las LC_* individuales     │
│  Usarla solo para forzar locale en contexto específico:               │
│  LC_ALL=C sort archivo.txt                                            │
└─────────────────────────────────────────────────────────────────────────┘
```

**Regla práctica:** `LANG` establece el default del sistema, `LC_*` individuales permiten ajustar aspectos concretos sin cambiar todo, y `LC_ALL` es la opción nuclear para forzar un locale completo (casi siempre de forma temporal en un comando o script).

---

## LANG, LC_ALL y C/POSIX

### LANG

```bash
# LANG define el locale por defecto del sistema:
echo $LANG
# en_US.UTF-8

# Cambiar permanentemente:
# Debian/Ubuntu:
sudo update-locale LANG=es_ES.UTF-8
# Modifica /etc/default/locale

# RHEL/Fedora:
sudo localectl set-locale LANG=es_ES.UTF-8
# Modifica /etc/locale.conf

# IMPORTANTE: el cambio aplica en la PRÓXIMA sesión (re-login)
```

### El locale C / POSIX

`C` y `POSIX` son sinónimos. Es el locale mínimo, integrado en la libc, y **siempre está disponible**:

```bash
# Características de C/POSIX:
# - ASCII puro (sin acentos, eñes, emojis)
# - Mensajes en inglés
# - Ordenamiento byte a byte (no lingüístico)
# - Sin formato regional (. como decimal, sin agrupamiento)

# C.UTF-8 — como C pero con soporte para caracteres UTF-8:
# - Reconoce caracteres multibyte
# - Pero sin convenciones regionales
# - Disponible en la mayoría de distribuciones modernas
```

### Cuándo usar C / POSIX

```bash
# 1. En scripts — para salida predecible:
LC_ALL=C sort archivo.txt
# Ordena byte a byte, sin sorpresas por locale

# 2. Parsear salida de comandos:
LC_ALL=C df -h | awk '{print $5}'
# Siempre usa . como separador decimal, no ,

# 3. Expresiones regulares predecibles:
LC_ALL=C grep '[a-z]' archivo
# Solo a-z ASCII, no incluye á, ñ, ü

# IMPORTANTE: [a-z] cambia de significado según el locale
echo "Ángel" | grep '[A-Z]'
# Con locale es_ES: puede coincidir o no (depende de LC_COLLATE)

echo "Ángel" | LC_ALL=C grep '[A-Z]'
# Solo coincide con 'A' (byte 0x41), Á es multibyte y no encaja
```

---

## Configuración del sistema

### Debian/Ubuntu

```bash
# Archivo de configuración:
cat /etc/default/locale
# LANG=en_US.UTF-8
# LC_MESSAGES=en_US.UTF-8

# Generar locales (compilar las definiciones):
sudo locale-gen es_ES.UTF-8
# Generating locales (this might take a while)...
#   es_ES.UTF-8... done

# O editar la lista de locales a generar:
cat /etc/locale.gen
# Descomentar las líneas deseadas:
# es_ES.UTF-8 UTF-8
# en_US.UTF-8 UTF-8
# pt_BR.UTF-8 UTF-8
sudo locale-gen    # regenerar todos

# Herramienta interactiva:
sudo dpkg-reconfigure locales
# Menú para seleccionar y configurar locales

# Cambiar el locale del sistema:
sudo update-locale LANG=es_ES.UTF-8
# Requiere re-login
```

### RHEL/Fedora

```bash
# Archivo de configuración:
cat /etc/locale.conf
# LANG="en_US.UTF-8"

# Cambiar con localectl:
sudo localectl set-locale LANG=es_ES.UTF-8

# Ver la configuración:
localectl
# System Locale: LANG=en_US.UTF-8
# VC Keymap: us
# X11 Layout: us

# Listar locales disponibles:
localectl list-locales | grep -i es
# es_AR.UTF-8
# es_ES.UTF-8
# es_MX.UTF-8

# En RHEL, los locales se instalan con paquetes glibc-langpack:
sudo dnf install glibc-langpack-es    # español
sudo dnf install glibc-all-langpacks  # todos los idiomas
```

### Listar locales disponibles

```bash
# Locales instalados/generados en el sistema:
locale -a
# C
# C.UTF-8
# en_US.utf8
# es_ES.utf8
# POSIX

# Si un locale no aparece, hay que generarlo (Debian) o instalarlo (RHEL)
```

### Comparación Debian vs RHEL

| Aspecto | Debian/Ubuntu | RHEL/Fedora |
|---------|---------------|-------------|
| Archivo de config | `/etc/default/locale` | `/etc/locale.conf` |
| Generar locales | `locale-gen`, `/etc/locale.gen` | `glibc-langpack-*` (paquetes RPM) |
| Cambiar default | `update-locale LANG=...` | `localectl set-locale LANG=...` |
| Menú interactivo | `dpkg-reconfigure locales` | (no hay equivalente) |
| Listar disponibles | `locale -a` | `localectl list-locales` |
| Aplica cambio | Re-login | Re-login |

---

## Problemas comunes en scripts

### Problema 1: sort con locale

```bash
# sort se comporta diferente según LC_COLLATE:

# Con locale C (orden byte):
echo -e "banana\nÁrbol\napple" | LC_ALL=C sort
# Cherry          ← C mayúscula: byte 0x43
# apple           ← a minúscula: byte 0x61
# banana
# Árbol           ← Á: bytes 0xC3 0x81 (multibyte, valor alto)

# Con locale es_ES (orden lingüístico):
echo -e "banana\nÁrbol\napple" | LC_ALL=es_ES.UTF-8 sort
# apple
# Árbol           ← Á se agrupa con la A
# banana

# REGLA: en scripts, usar LC_ALL=C para ordenamiento predecible
```

### Problema 2: grep con clases de caracteres

```bash
# [a-z] NO es lo mismo en todos los locales:

# En C/POSIX:
echo "café" | LC_ALL=C grep -o '[a-z]*'
# caf             ← é no es [a-z] en ASCII

# En es_ES:
echo "café" | LC_ALL=es_ES.UTF-8 grep -o '[a-z]*'
# café            ← é es una letra minúscula en español

# Alternativas seguras — clases POSIX:
echo "café" | grep -o '[[:alpha:]]*'   # POSIX: cualquier letra (respeta locale)
echo "café" | grep -oP '[\p{Ll}]+'    # PCRE: lowercase letter (Unicode)
```

### Problema 3: separador decimal

```bash
# Algunos locales usan coma como separador decimal:
LC_NUMERIC=de_DE.UTF-8 printf "%'.2f\n" 1234567.89
# 1.234.567,89

LC_NUMERIC=en_US.UTF-8 printf "%'.2f\n" 1234567.89
# 1,234,567.89

# En scripts que parsean números:
# SIEMPRE usar LC_NUMERIC=C para que . sea el decimal
VALUE=$(LC_ALL=C awk '{print $1 * 1.5}' <<< "3.14")
echo "$VALUE"
# 4.71
```

---

## Buenas prácticas para scripts

```bash
#!/bin/bash
# Patrón seguro para scripts:

# 1. Guardar locale del usuario
USER_LC_ALL="$LC_ALL"

# 2. Operaciones internas en C (predecible):
export LC_ALL=C

# Parsear, ordenar, grep — todo predecible:
RESULT=$(some_command | grep pattern | awk '{print $3}')

# 3. Restaurar para mensajes al usuario:
export LC_ALL="$USER_LC_ALL"
echo "Resultado: $RESULT"

# 4. Si necesitas locale específico solo para un comando:
LC_TIME=en_US.UTF-8 date "+%B %d, %Y"
```

**Resumen de qué variable usar según el caso:**

| Situación | Variable | Valor |
|-----------|----------|-------|
| Ordenar texto en script | `LC_ALL=C` o `LC_COLLATE=C` | Orden byte a byte |
| Regex con rangos `[a-z]` | `LC_ALL=C` o `LC_CTYPE=C` | Solo ASCII |
| Parsear números con `.` | `LC_ALL=C` o `LC_NUMERIC=C` | Punto como decimal |
| Parsear mensajes de error | `LC_ALL=C` o `LC_MESSAGES=C` | Inglés predecible |
| Mostrar fechas al usuario | `LC_TIME=es_ES.UTF-8` | Formato regional |
| Todo predecible | `export LC_ALL=C` al inicio | Script completo en C |

---

## UTF-8

```bash
# UTF-8 es la codificación estándar en Linux moderno
# Soporta todos los caracteres Unicode (emojis, CJK, árabe, etc.)

# Verificar que el sistema usa UTF-8:
locale charmap
# UTF-8

# Si no es UTF-8, convertir:
sudo update-locale LANG=en_US.UTF-8    # Debian
sudo localectl set-locale LANG=en_US.UTF-8   # RHEL
```

### Convertir archivos a UTF-8

```bash
# Ver codificación actual:
file archivo.txt
# archivo.txt: ISO-8859-1 text

# Convertir a UTF-8:
iconv -f ISO-8859-1 -t UTF-8 archivo.txt > archivo-utf8.txt

# Con transliteración (intenta convertir caracteres no soportados):
iconv -f LATIN1 -t UTF-8//TRANSLIT archivo.txt -o archivo-utf8.txt

# Detectar codificación automáticamente:
enca archivo.txt    # requiere paquete enca
```

---

## Diagnóstico de problemas de locale

```bash
# PROBLEMA: caracteres extraños en la terminal (mojibake)
#
# 1. Verificar locale actual:
echo $LANG
locale charmap
#
# 2. Verificar que el locale está instalado:
locale -a | grep -i utf
#
# 3. Si el locale no está instalado:
sudo locale-gen "$LANG"                  # Debian
sudo dnf install glibc-langpack-es       # RHEL
#
# 4. Regenerar todos los locales:
sudo dpkg-reconfigure locales            # Debian

# PROBLEMA: sort/grep devuelve resultados inesperados
#
# Causa: LC_COLLATE o LC_CTYPE diferente al esperado
# Solución: forzar C locale en la operación:
LC_ALL=C sort archivo.txt
LC_ALL=C grep '[a-z]' archivo.txt
```

---

## Quick reference

```
VER LOCALE:
  locale              → todas las variables
  locale charmap      → codificación (debe ser UTF-8)
  locale -a           → locales instalados
  localectl           → configuración del sistema (RHEL)

CAMBIAR:
  Debian:  sudo update-locale LANG=es_ES.UTF-8
  RHEL:    sudo localectl set-locale LANG=es_ES.UTF-8

GENERAR LOCALE:
  Debian:  sudo locale-gen es_ES.UTF-8
  RHEL:    sudo dnf install glibc-langpack-es

PRECEDENCIA:
  LC_ALL > LC_* individuales > LANG

EN SCRIPTS:
  LC_ALL=C sort archivo.txt          → orden byte a byte
  LC_ALL=C grep '[a-z]' archivo      → regex predecible
  LC_ALL=C awk '{print $1}'          → números predecibles

UTF-8:
  file archivo.txt                   → ver codificación
  iconv -f ISO-8859-1 -t UTF-8 in > out → convertir
```

---

## Labs

### Lab Parte 1 — Variables LC_* y precedencia

**Objetivo:** Entender las variables de locale y su precedencia.

#### Paso 1.1: Locale actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Locale actual ==="
locale
echo ""
echo "--- Formato del nombre ---"
echo "  idioma_TERRITORIO.CODIFICACION"
echo "  es_ES.UTF-8  = español de España, UTF-8"
echo "  en_US.UTF-8  = inglés de EEUU, UTF-8"
echo ""
echo "El territorio importa:"
echo "  es_ES → moneda: EUR"
echo "  es_MX → moneda: MXN"
echo "  en_US → fecha: 03/17/2026"
echo "  en_GB → fecha: 17/03/2026"
'
```

<details><summary>Predicción</summary>

`locale` muestra todas las variables `LC_*`. `LANG` debería estar definido (probablemente `en_US.UTF-8` o `C.UTF-8` según la imagen Docker). Todas las `LC_*` entre comillas heredan de `LANG`. `LC_ALL=` vacío significa que no sobreescribe nada.

</details>

#### Paso 1.2: Efecto de LC_TIME y LC_MESSAGES

```bash
docker compose exec debian-dev bash -c '
echo "--- Efecto de LC_TIME ---"
echo "  C:      $(LC_ALL=C date +"%A")"
echo "  en_US:  $(LC_ALL=en_US.UTF-8 date +"%A" 2>/dev/null || echo "(no disponible)")"
echo "  es_ES:  $(LC_ALL=es_ES.UTF-8 date +"%A" 2>/dev/null || echo "(no disponible)")"
echo ""
echo "--- Efecto de LC_MESSAGES ---"
echo "  C:      $(LC_ALL=C ls /noexiste 2>&1)"
echo "  es_ES:  $(LC_ALL=es_ES.UTF-8 ls /noexiste 2>&1)"
'
```

<details><summary>Predicción</summary>

`LC_TIME` cambia el nombre del día de la semana: con `C` sale en inglés (e.g. `Wednesday`), con `es_ES` sale en español (`miércoles`) — si ese locale está instalado. `LC_MESSAGES` cambia el idioma del mensaje de error de `ls`. Si el locale no está generado, `date` puede dar warning o usar el fallback C.

</details>

#### Paso 1.3: Precedencia

```bash
docker compose exec debian-dev bash -c '
echo "=== Precedencia ==="
echo "  LANG=$(locale | grep ^LANG= | cut -d= -f2)"
echo "  LC_ALL=$(locale | grep ^LC_ALL= | cut -d= -f2)"
echo ""
LANG_VAL=$(locale | grep "^LANG=" | cut -d= -f2 | tr -d "\"")
DIFF=$(locale | grep -v "^LANG=\|^LC_ALL=" | grep -v "\"$LANG_VAL\"" | grep -v "=$LANG_VAL" | grep -cv "^$" 2>/dev/null || echo 0)
echo "  Variables LC_* diferentes de LANG: $DIFF"
echo ""
echo "--- Regla ---"
echo "  1. LC_ALL     → sobreescribe TODO"
echo "  2. LC_*       → cada variable individual"
echo "  3. LANG       → valor por defecto"
'
```

<details><summary>Predicción</summary>

En un contenedor típico, `LANG` tiene un valor (e.g. `en_US.UTF-8`) y `LC_ALL` está vacío. Todas las `LC_*` individuales heredan de `LANG`, así que el conteo de variables diferentes debería ser `0`. Si el host exporta alguna `LC_*` específica, podría diferir.

</details>

#### Paso 1.4: Locale C / POSIX

```bash
docker compose exec debian-dev bash -c '
echo "=== Locale C / POSIX ==="
echo ""
echo "C (o POSIX): locale mínimo y predecible"
echo "  - ASCII puro, mensajes en inglés"
echo "  - Ordenamiento byte a byte"
echo "  - Siempre disponible"
echo ""
echo "C.UTF-8: como C pero con soporte UTF-8"
echo ""
echo "--- Locales disponibles ---"
locale -a 2>/dev/null | head -10
echo "..."
echo "Total: $(locale -a 2>/dev/null | wc -l) locales"
'
```

<details><summary>Predicción</summary>

`locale -a` muestra todos los locales generados. Siempre aparecen `C` y `POSIX`. Según los paquetes instalados en el contenedor, puede haber pocos (solo C, C.UTF-8, POSIX) o muchos. En un contenedor Debian mínimo, típicamente hay entre 3-5 locales básicos a menos que se haya ejecutado `locale-gen` para otros.

</details>

### Lab Parte 2 — Configuración del sistema

**Objetivo:** Configurar locales en Debian y RHEL.

#### Paso 2.1: Debian

```bash
docker compose exec debian-dev bash -c '
echo "=== Configuración en Debian ==="
echo ""
echo "--- /etc/default/locale ---"
if [[ -f /etc/default/locale ]]; then
    cat /etc/default/locale
else
    echo "  (no encontrado)"
fi
echo ""
echo "--- /etc/locale.gen ---"
if [[ -f /etc/locale.gen ]]; then
    echo "Locales activos (descomentados):"
    grep -v "^#" /etc/locale.gen | grep -v "^$" | head -5
    echo "Total activos: $(grep -v "^#" /etc/locale.gen | grep -v "^$" | wc -l)"
else
    echo "  (no encontrado)"
fi
echo ""
echo "--- Comandos ---"
echo "  locale-gen es_ES.UTF-8           generar un locale"
echo "  dpkg-reconfigure locales         menú interactivo"
echo "  update-locale LANG=es_ES.UTF-8   cambiar default"
'
```

<details><summary>Predicción</summary>

`/etc/default/locale` debería existir con al menos `LANG=...`. `/etc/locale.gen` lista los locales a compilar; las líneas descomentadas son los que están activos. En un contenedor mínimo, puede que solo haya uno o dos locales activos (e.g. `en_US.UTF-8 UTF-8`).

</details>

#### Paso 2.2: RHEL

```bash
docker compose exec alma-dev bash -c '
echo "=== Configuración en RHEL ==="
echo ""
echo "--- /etc/locale.conf ---"
if [[ -f /etc/locale.conf ]]; then
    cat /etc/locale.conf
else
    echo "  (no encontrado)"
fi
echo ""
echo "--- Locales disponibles ---"
locale -a 2>/dev/null | head -10
echo "..."
echo "Total: $(locale -a 2>/dev/null | wc -l)"
echo ""
echo "--- Paquetes de idioma instalados ---"
rpm -qa glibc-langpack* 2>/dev/null | head -5 || echo "  (verificar con rpm)"
echo ""
echo "--- Comandos ---"
echo "  localectl set-locale LANG=es_ES.UTF-8"
echo "  dnf install glibc-langpack-es    (español)"
echo "  dnf install glibc-all-langpacks  (todos)"
'
```

<details><summary>Predicción</summary>

`/etc/locale.conf` debería existir con `LANG="en_US.UTF-8"` o similar. Los paquetes `glibc-langpack-*` instalados determinan qué locales están disponibles. En AlmaLinux mínimo, probablemente solo hay `glibc-langpack-en` instalado. `locale -a` mostrará solo los locales del paquete instalado.

</details>

#### Paso 2.3: Comparación Debian vs RHEL

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian vs RHEL ==="
echo ""
echo "| Aspecto             | Debian                      | RHEL                        |"
echo "|---------------------|-----------------------------|-----------------------------|"
echo "| Config              | /etc/default/locale         | /etc/locale.conf            |"
echo "| Generar             | locale-gen, /etc/locale.gen  | glibc-langpack-* (RPM)      |"
echo "| Cambiar             | update-locale               | localectl set-locale        |"
echo "| Menú interactivo    | dpkg-reconfigure locales    | (no hay)                    |"
echo "| Listar              | locale -a                   | localectl list-locales      |"
'
```

<details><summary>Predicción</summary>

Solo imprime la tabla de comparación. La diferencia fundamental: Debian **compila** locales desde definiciones de texto (`locale-gen`), RHEL los **instala** como paquetes RPM (`glibc-langpack-*`). Ambos requieren re-login para que el cambio de `LANG` aplique.

</details>

### Lab Parte 3 — Implicaciones en scripts

**Objetivo:** Entender cómo el locale afecta el comportamiento de comandos.

#### Paso 3.1: sort y locale

```bash
docker compose exec debian-dev bash -c '
echo "=== sort y locale ==="
DATA="banana
apple
Cherry
date"

echo "Datos: banana, apple, Cherry, date"
echo ""
echo "--- LC_ALL=C ---"
echo "$DATA" | LC_ALL=C sort
echo ""
echo "--- LC_ALL=en_US.UTF-8 ---"
echo "$DATA" | LC_ALL=en_US.UTF-8 sort 2>/dev/null || echo "(locale no disponible)"
echo ""
echo "C: ordena por valor de byte (A-Z antes que a-z)"
echo "en_US: ordena lingüísticamente (a y A juntos)"
'
```

<details><summary>Predicción</summary>

Con `LC_ALL=C`: primero las mayúsculas (`Cherry`) porque `C` < `a` en ASCII (0x43 < 0x61), luego `apple`, `banana`, `date`. Con `en_US.UTF-8`: ordena ignorando mayúsculas/minúsculas — `apple`, `banana`, `Cherry`, `date` (orden lingüístico alfabético). La diferencia clave: C ordena por valor de byte, en_US ordena por significado lingüístico.

</details>

#### Paso 3.2: grep y clases de caracteres

```bash
docker compose exec debian-dev bash -c '
echo "=== grep y locale ==="
echo ""
echo "--- En C ---"
echo "Buscar [a-z]* en \"café\":"
echo "cafe" | LC_ALL=C grep -o "[a-z]*"
echo "  (solo a-z ASCII)"
echo ""
echo "--- Alternativas seguras ---"
echo "  [[:alpha:]]  cualquier letra (respeta locale)"
echo "  [[:lower:]]  minúsculas (respeta locale)"
echo "  [[:upper:]]  mayúsculas (respeta locale)"
echo "  [[:digit:]]  dígitos"
echo "  Para ASCII estricto: usar LC_ALL=C"
'
```

<details><summary>Predicción</summary>

Con `LC_ALL=C`, `grep -o '[a-z]*'` sobre "cafe" devuelve `cafe` (todas son ASCII). Si el input fuera "café" con acento, devolvería `caf` (sin la é). Las clases POSIX como `[[:alpha:]]` se adaptan al locale — en `es_ES` incluirían é, ñ, etc.

</details>

#### Paso 3.3: Separador decimal

```bash
docker compose exec debian-dev bash -c '
echo "=== Separador decimal ==="
echo ""
echo "SIEMPRE usar LC_ALL=C o LC_NUMERIC=C en scripts que parsean números"
echo ""
echo "Ejemplo seguro:"
VALUE=$(LC_ALL=C awk "{print \$1 * 1.5}" <<< "3.14")
echo "  LC_ALL=C awk \"{print \$1 * 1.5}\" <<< \"3.14\""
echo "  Resultado: $VALUE"
echo ""
echo "Si LC_NUMERIC no es C, el punto podría no parsearse como decimal"
echo "y el cálculo fallaría o daría resultados incorrectos."
'
```

<details><summary>Predicción</summary>

Con `LC_ALL=C`, awk parsea `3.14` correctamente (`.` es decimal) y calcula `3.14 * 1.5 = 4.71`. En un locale como `de_DE.UTF-8` donde `,` es el separador decimal, awk podría interpretar `3.14` como `3` (truncando en el `.`) y dar `4.5` en lugar de `4.71`.

</details>

#### Paso 3.4: UTF-8

```bash
docker compose exec debian-dev bash -c '
echo "=== UTF-8 ==="
echo ""
echo "Codificación actual: $(locale charmap 2>/dev/null || echo "desconocida")"
echo ""
echo "--- Convertir archivos ---"
echo "  file archivo.txt                  → ver codificación"
echo "  iconv -f ISO-8859-1 -t UTF-8 in > out  → convertir"
echo ""
echo "--- Tabla resumen de impacto ---"
echo ""
echo "| Variable     | Afecta a         | Cuidado con              |"
echo "|--------------|------------------|--------------------------|"
echo "| LC_COLLATE   | sort, ls, glob   | Ordenamiento inesperado  |"
echo "| LC_CTYPE     | regex, tr, wc    | [a-z] incluye acentos?   |"
echo "| LC_NUMERIC   | printf, awk      | . vs , como decimal      |"
echo "| LC_MESSAGES  | mensajes de error| Parsear errores          |"
echo "| LC_TIME      | date             | Formato de fecha         |"
echo "| LC_ALL       | TODO             | Sobreescribe todo        |"
'
```

<details><summary>Predicción</summary>

`locale charmap` debería devolver `UTF-8` en cualquier distribución moderna. La tabla resumen muestra la variable específica que causa cada problema — la clave es que `LC_ALL=C` los resuelve todos a la vez para operaciones internas de scripts, pero a costa de perder soporte para caracteres no-ASCII.

</details>

---

## Ejercicios

### Ejercicio 1 — Explorar el locale actual

```bash
# 1. Muestra tu LANG actual
echo "LANG=$LANG"

# 2. ¿Qué codificación usas?
locale charmap

# 3. ¿Hay alguna variable LC_* con valor diferente de LANG?
locale

# 4. ¿Cuántos locales están instalados en tu sistema?
locale -a | wc -l
```

<details><summary>Predicción</summary>

1. `LANG` mostrará el locale configurado del sistema (e.g. `en_US.UTF-8`, `es_ES.UTF-8`).
2. `locale charmap` debería mostrar `UTF-8` en cualquier sistema moderno.
3. `locale` muestra todas las variables. Las que están entre comillas (`"en_US.UTF-8"`) heredan de `LANG`. Si `LC_ALL` está vacío, no sobreescribe nada.
4. El conteo varía: un sistema mínimo tiene ~3-5 (C, C.UTF-8, POSIX, en_US.utf8), un escritorio puede tener 100+.

</details>

### Ejercicio 2 — Efecto de LC_TIME en date

```bash
# Usa date con %x (formato de fecha local) en diferentes locales:
echo "=== C ==="
LC_ALL=C date +"%x"

echo "=== en_US ==="
LC_ALL=en_US.UTF-8 date +"%x" 2>/dev/null || echo "No disponible"

echo "=== es_ES ==="
LC_ALL=es_ES.UTF-8 date +"%x" 2>/dev/null || echo "No disponible"

echo "=== de_DE ==="
LC_ALL=de_DE.UTF-8 date +"%x" 2>/dev/null || echo "No disponible"
```

<details><summary>Predicción</summary>

`%x` produce el formato de fecha "local" según el locale:
- **C**: `03/25/2026` (MM/DD/YYYY)
- **en_US**: `03/25/2026` (mismo formato americano)
- **es_ES**: `25/03/2026` (DD/MM/YYYY, formato europeo)
- **de_DE**: `25.03.2026` (DD.MM.YYYY, formato alemán con puntos)

Si el locale no está instalado, muestra "No disponible". El formato `%x` es uno de los que más cambian entre locales.

</details>

### Ejercicio 3 — Ordenamiento con sort

```bash
# Compara el ordenamiento de estas palabras en diferentes locales:
DATA="banana
Árbol
apple
Ñoño
zebra
Cherry"

echo "=== LC_ALL=C ==="
echo "$DATA" | LC_ALL=C sort

echo ""
echo "=== LC_ALL=en_US.UTF-8 ==="
echo "$DATA" | LC_ALL=en_US.UTF-8 sort 2>/dev/null || echo "No disponible"

echo ""
echo "=== LC_ALL=es_ES.UTF-8 ==="
echo "$DATA" | LC_ALL=es_ES.UTF-8 sort 2>/dev/null || echo "No disponible"
```

<details><summary>Predicción</summary>

- **C**: Orden byte a byte. Mayúsculas primero (Cherry), luego minúsculas (apple, banana, zebra), luego multibyte (Árbol con bytes 0xC3 0x81, Ñoño con bytes 0xC3 0x91 — ambos muy al final).
- **en_US**: Orden lingüístico. apple, Árbol (Á con A), banana, Cherry, Ñoño (posiblemente después de N o al final), zebra.
- **es_ES**: Similar a en_US, pero Ñ tiene posición especial entre N y O en el collation español.

</details>

### Ejercicio 4 — grep y [a-z] según locale

```bash
# Prueba cómo [a-z] cambia según el locale:
echo "=== Con LC_ALL=C ==="
echo "café résumé naïve" | LC_ALL=C grep -o '[a-z]*'

echo ""
echo "=== Con LC_ALL=en_US.UTF-8 ==="
echo "café résumé naïve" | LC_ALL=en_US.UTF-8 grep -o '[a-z]*' 2>/dev/null

echo ""
echo "=== Con [[:alpha:]] (clase POSIX) ==="
echo "café résumé naïve" | grep -o '[[:alpha:]]*'
```

<details><summary>Predicción</summary>

- **C**: `[a-z]` solo reconoce ASCII a-z. Resultado: `caf`, `r`, `sum`, `na`, `ve` (se corta en cada carácter acentuado porque los bytes multibyte no coinciden).
- **en_US.UTF-8**: `[a-z]` puede incluir caracteres acentuados según la implementación de `LC_COLLATE`. Posiblemente devuelva las palabras completas.
- **[[:alpha:]]**: Clase POSIX que siempre respeta el locale. Devuelve `café`, `résumé`, `naïve` completas (reconoce acentos como letras).

</details>

### Ejercicio 5 — Precedencia de LC_ALL

```bash
# Demuestra que LC_ALL anula todo:

# Sin LC_ALL — LC_TIME define el formato de fecha:
LANG=en_US.UTF-8 LC_TIME=es_ES.UTF-8 date +"%A" 2>/dev/null
# Debería mostrar el día en español

# Con LC_ALL — ignora LC_TIME:
LANG=en_US.UTF-8 LC_TIME=es_ES.UTF-8 LC_ALL=C date +"%A" 2>/dev/null
# Debería mostrar el día en inglés (C sobreescribe todo)

# LC_ALL vacío — no sobreescribe:
LANG=es_ES.UTF-8 LC_ALL= date +"%A" 2>/dev/null
# Debería mostrar el día en español (LANG aplica)
```

<details><summary>Predicción</summary>

1. Sin LC_ALL: `LC_TIME=es_ES.UTF-8` toma precedencia sobre `LANG` para fechas → muestra el día en español (e.g. `miércoles`).
2. Con `LC_ALL=C`: ignora tanto `LANG` como `LC_TIME` → muestra en inglés (e.g. `Wednesday`).
3. `LC_ALL=` (vacío): no sobreescribe nada, así que `LANG=es_ES.UTF-8` aplica como default → día en español.

Esto demuestra la jerarquía: `LC_ALL` > `LC_*` > `LANG`.

</details>

### Ejercicio 6 — Generar un locale (Debian)

```bash
# En debian-dev, verifica qué locales existen:
docker compose exec debian-dev bash -c '
echo "=== Locales antes ==="
locale -a

echo ""
echo "=== Generando es_MX.UTF-8 ==="
locale-gen es_MX.UTF-8 2>&1

echo ""
echo "=== Locales después ==="
locale -a

echo ""
echo "=== Probando ==="
LC_ALL=es_MX.UTF-8 date +"%A %d de %B de %Y" 2>/dev/null || echo "Falló"
'
```

<details><summary>Predicción</summary>

Antes de `locale-gen`, `es_MX.UTF-8` no aparece en `locale -a`. Después de ejecutar `locale-gen es_MX.UTF-8`, se compila y aparece como `es_MX.utf8` en la lista. `date` con ese locale debería mostrar el día y mes en español (e.g. `miércoles 25 de marzo de 2026`). Si `es_MX` no está en `/etc/locale.gen`, `locale-gen` puede requerir que se agregue primero.

</details>

### Ejercicio 7 — Instalar locale en RHEL

```bash
# En alma-dev, verifica y prueba instalar un locale:
docker compose exec alma-dev bash -c '
echo "=== Locales instalados ==="
locale -a 2>/dev/null | head -10
echo "Total: $(locale -a 2>/dev/null | wc -l)"

echo ""
echo "=== Paquetes glibc-langpack instalados ==="
rpm -qa glibc-langpack* 2>/dev/null | sort

echo ""
echo "=== Probando es_ES.UTF-8 ==="
LC_ALL=es_ES.UTF-8 date +"%A" 2>/dev/null || echo "Locale no disponible (instalar glibc-langpack-es)"
'
```

<details><summary>Predicción</summary>

Si solo `glibc-langpack-en` está instalado, `locale -a` mostrará pocos locales (C, POSIX, en_US.utf8). `es_ES.UTF-8` no estará disponible y `date` mostrará el fallback o error. Se necesitaría `dnf install glibc-langpack-es` para habilitarlo. La diferencia con Debian: RHEL no "compila" locales, los instala como paquetes RPM.

</details>

### Ejercicio 8 — Separador decimal en awk

```bash
# Demuestra el riesgo del separador decimal:

# Cálculo correcto (C locale):
CORRECT=$(LC_ALL=C awk 'BEGIN {printf "%.2f", 3.14 * 2.5}')
echo "Con LC_ALL=C: 3.14 * 2.5 = $CORRECT"

# Intenta con locale que usa coma (si disponible):
MAYBE_WRONG=$(LC_ALL=de_DE.UTF-8 awk 'BEGIN {printf "%.2f", 3.14 * 2.5}' 2>/dev/null)
echo "Con LC_ALL=de_DE: 3.14 * 2.5 = ${MAYBE_WRONG:-(locale no disponible)}"

# Valor seguro para scripts:
echo ""
echo "Regla: SIEMPRE usar LC_ALL=C o LC_NUMERIC=C al parsear números en scripts"
```

<details><summary>Predicción</summary>

- Con `LC_ALL=C`: `3.14 * 2.5 = 7.85` (correcto, `.` es el decimal).
- Con `LC_ALL=de_DE.UTF-8`: Depende de la implementación de awk. GNU awk moderno respeta `LC_NUMERIC` para `printf` pero parsea con `.` internamente, así que probablemente produce `7,85` (con coma en la salida) o `7.85`. El riesgo real aparece cuando se **lee** un valor con coma de un archivo y awk lo trunca en la coma.

</details>

### Ejercicio 9 — Script robusto con locale

```bash
# Escribe un script que funcione correctamente independientemente del locale:

#!/bin/bash
# Guardar locale del usuario
SAVED_LC_ALL="${LC_ALL:-}"

# Fase 1: operaciones internas (predecible)
export LC_ALL=C
DISK_USAGE=$(df -h / | awk 'NR==2 {print $5}')
MEM_AVAIL=$(free -m | awk '/^Mem:/ {print $7}')
LOAD=$(uptime | grep -oP 'load average: \K[0-9.]+')

# Fase 2: restaurar locale para output al usuario
if [[ -n "$SAVED_LC_ALL" ]]; then
    export LC_ALL="$SAVED_LC_ALL"
else
    unset LC_ALL
fi

echo "Disco /: $DISK_USAGE usado"
echo "RAM disponible: ${MEM_AVAIL}MB"
echo "Load average: $LOAD"
```

<details><summary>Predicción</summary>

El script funciona en dos fases:
1. **LC_ALL=C** para parsear: `df`, `free`, `uptime` producen salida en inglés con `.` como decimal. `awk` extrae campos de forma predecible. Sin C, un locale con coma decimal podría romper el parseo de `$5` en `df`.
2. **Restaurar locale** para los `echo`: los mensajes al usuario aparecen en su idioma/formato. Si `SAVED_LC_ALL` estaba vacío, se hace `unset` (no `export LC_ALL=""`, que sobreescribiría con cadena vacía).

Resultado: tres líneas con uso de disco (%), RAM disponible (MB), y load average.

</details>

### Ejercicio 10 — Diagnóstico de problemas de locale

```bash
# Diagnostica y resuelve un problema de locale:

# 1. Verificar estado actual
echo "=== Estado actual ==="
echo "LANG=$LANG"
echo "LC_ALL=${LC_ALL:-(vacío)}"
echo "Charmap: $(locale charmap 2>/dev/null)"

# 2. Buscar locales UTF-8 disponibles
echo ""
echo "=== Locales UTF-8 disponibles ==="
locale -a 2>/dev/null | grep -i utf | head -10

# 3. Probar si un locale específico funciona
echo ""
echo "=== Test de locale ==="
for LOC in C C.UTF-8 en_US.UTF-8 es_ES.UTF-8; do
    if LC_ALL=$LOC date +"%x" > /dev/null 2>&1; then
        echo "  $LOC: OK ($(LC_ALL=$LOC date +"%x"))"
    else
        echo "  $LOC: NO DISPONIBLE"
    fi
done

# 4. Verificar que el archivo de config es consistente
echo ""
echo "=== Archivo de configuración ==="
if [[ -f /etc/default/locale ]]; then
    echo "Debian: $(cat /etc/default/locale)"
elif [[ -f /etc/locale.conf ]]; then
    echo "RHEL: $(cat /etc/locale.conf)"
else
    echo "No se encontró archivo de configuración"
fi
```

<details><summary>Predicción</summary>

El script diagnostica en 4 pasos:
1. Muestra `LANG` y `LC_ALL` actuales, y la codificación (debe ser `UTF-8`).
2. Lista los locales UTF-8 disponibles — si está vacío, hay que generar/instalar locales.
3. Prueba cada locale: `C` y `C.UTF-8` siempre funcionan; `en_US.UTF-8` probablemente sí; `es_ES.UTF-8` puede fallar si no está generado/instalado.
4. Muestra el archivo de config según la distro. Si `LANG` en el archivo no coincide con un locale disponible en `locale -a`, el sistema tiene un problema de configuración.

</details>

