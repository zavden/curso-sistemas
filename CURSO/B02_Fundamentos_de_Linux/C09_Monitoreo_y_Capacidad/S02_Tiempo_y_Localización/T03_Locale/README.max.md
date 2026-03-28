# T03 — Locale

> **Objetivo:** Entender cómo Linux maneja los locales, configurar el idioma y formato regional, y evitar problemas comunes con codificaciones y ordenamiento.

## Qué es un locale

Un locale define las **convenciones regionales** del sistema:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  LOCALE = idioma + territorio + codificación                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  idioma_TERRITORIO.CODIFICACIÓN                                            │
│                                                                              │
│  es_ES.UTF-8   → Español de España, UTF-8                                 │
│  en_US.UTF-8   → Inglés de Estados Unidos, UTF-8                           │
│  pt_BR.UTF-8   → Portugués de Brasil, UTF-8                              │
│  de_DE.UTF-8   → Alemán de Alemania, UTF-8                                │
│                                                                              │
│  El TERRITORIO importa:                                                   │
│  es_ES → fecha: 17/03/2026, moneda: €                                  │
│  es_MX → fecha: 17/03/2026, moneda: $                                  │
│  en_US → fecha: 03/17/2026, moneda: $                                  │
│  en_GB → fecha: 17/03/2026, moneda: £                                  │
└─────────────────────────────────────────────────────────────────────────────┘
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
| `LANG` | Default para todas las LC_* | Catch-all |
| `LC_CTYPE` | Clasificación de caracteres | isalpha(), mayúsculas/minúsculas |
| `LC_NUMERIC` | Formato de números | Separador decimal (`.` vs `,`) |
| `LC_TIME` | Formato de fechas | `17/03/2026` vs `03/17/2026` |
| `LC_COLLATE` | Ordenamiento de texto | `Á` después de `A` o de `Z` |
| `LC_MONETARY` | Formato de moneda | `$100.00` vs `100,00 €` |
| `LC_MESSAGES` | Idioma de mensajes | "File not found" vs "Archivo no encontrado" |
| `LC_PAPER` | Tamaño de papel | A4 vs Letter |
| `LC_NAME` | Formato de nombres | |
| `LC_ADDRESS` | Formato de direcciones | |
| `LC_TELEPHONE` | Formato de teléfonos | |
| `LC_MEASUREMENT` | Sistema de medidas | métrico vs imperial |
| `LC_ALL` | Override total | Ignora todas las anteriores |

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
```

### Precedencia de variables

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  PRECEDENCIA (de mayor a menor)                                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  1. LC_ALL        → override total (ignora TODO)                        │
│  2. LC_*          → cada variable individual                             │
│  3. LANG          → valor por defecto                                    │
│                                                                              │
│  Ejemplo:                                                                  │
│  LANG=es_ES.UTF-8         → default: español                            │
│  LC_TIME=en_US.UTF-8       → pero fechas en inglés                        │
│  LC_ALL=                   → reset a valores por defecto                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## LANG, LC_ALL y C/POSIX

### LANG

```bash
# LANG define el locale por defecto del sistema:

# Cambiar permanentemente:
# Debian/Ubuntu:
sudo update-locale LANG=es_ES.UTF-8

# RHEL/Fedora:
sudo localectl set-locale LANG=es_ES.UTF-8

# Requiere re-login para aplicar
```

### El locale C / POSIX

```bash
# C (o POSIX) es el locale mínimo y predecible:
# - ASCII puro (sin acentos, eñes, emojis)
# - Mensajes en inglés
# - Ordenamiento byte a byte
# - Sin formato regional

# C.UTF-8 — como C pero con UTF-8:
# - Caracteres UTF-8 válidos
# - Sin convenciones regionales
```

### Cuándo usar C / POSIX

```bash
# 1. Scripts — para salida predecible:
LC_ALL=C sort archivo.txt
# Ordena byte a byte, sin locale

# 2. Parsear salida de comandos:
LC_ALL=C df -h | awk '{print $5}'
# Siempre usa . como separador decimal

# 3. Expresiones regulares:
LC_ALL=C grep '[a-z]' archivo
# Solo a-z ASCII, sin caracteres multibyte

# Con locale normal:
echo "Ángel" | grep '[A-Z]'
# Puede dar resultados inesperados

echo "Ángel" | LC_ALL=C grep '[A-Z]'
# Solo coincide 'A' (byte 0x41), Á es multibyte
```

---

## Configuración del sistema

### Debian/Ubuntu

```bash
# Archivo de configuración:
cat /etc/default/locale
# LANG=en_US.UTF-8

# Generar locales:
sudo locale-gen es_ES.UTF-8

# O editar /etc/locale.gen y descomentar:
# es_ES.UTF-8 UTF-8
sudo locale-gen

# Herramienta interactiva:
sudo dpkg-reconfigure locales

# Cambiar locale:
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

# Ver configuración:
localectl
# System Locale: LANG=en_US.UTF-8

# Listar locales disponibles:
localectl list-locales | grep -i es
# es_AR.UTF-8
# es_CL.UTF-8
# es_CO.UTF-8
# es_ES.UTF-8
# es_MX.UTF-8

# Instalar locales:
sudo dnf install glibc-langpack-es    # español
sudo dnf install glibc-all-langpacks  # todos
```

### Listar locales disponibles

```bash
# Locales instalados/generados:
locale -a
# C
# C.UTF-8
# en_US.utf8
# es_ES.utf8
# POSIX

# Si un locale no aparece, hay que generarlo/instalarlo
```

---

## Problemas comunes en scripts

### Problema 1: sort con locale

```bash
# sort se comporta diferente según LC_COLLATE:

echo -e "banana\nÁrbol\napple" | LC_ALL=C sort
# apple
# banana
# Árbol         ← orden byte (Á tiene valor alto)

echo -e "banana\nÁrbol\napple" | LC_ALL=es_ES.UTF-8 sort
# apple
# Árbol         ← orden lingüístico (Á con A)
# banana

# REGLA: en scripts, usar LC_ALL=C para ordenamiento predecible
```

### Problema 2: grep con clases de caracteres

```bash
# [a-z] no es lo mismo en todos los locales:

echo "café" | LC_ALL=C grep -o '[a-z]*'
# caf         ← é no es [a-z] en ASCII

echo "café" | LC_ALL=es_ES.UTF-8 grep -o '[a-z]*'
# café        ← é es letra en español

# Solución segura — clases POSIX:
echo "café" | grep -o '[[:alpha:]]*'
# café
```

### Problema 3: separador decimal

```bash
# Algunos locales usan coma como separador decimal:

LC_NUMERIC=de_DE.UTF-8 printf "%'.2f\n" 1234567.89
# 1.234.567,89

LC_NUMERIC=en_US.UTF-8 printf "%'.2f\n" 1234567.89
# 1,234,567.89

# En scripts que parsean números:
VALUE=$(LC_ALL=C awk '{print $1 * 1.5}' <<< "3.14")
# SIEMPRE usar LC_NUMERIC=C para evitar sorpresas
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

# 4. Si necesitas locale específico para output:
LC_TIME=en_US.UTF-8 date "+%B %d, %Y"
```

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

# O con recodificación:
iconv -f LATIN1 -t UTF-8//TRANSLIT archivo.txt -o archivo-utf8.txt
# //TRANSLIT intenta transliterar caracteres no soportados

# Detectar codificación automáticamente:
enca archivo.txt
```

---

## Diagnóstico de problemas de locale

```bash
# PROBLEMA: caracteres extraños en la terminal

# 1. Verificar locale actual:
echo $LANG
locale charmap

# 2. Verificar que la terminal soporta UTF-8:
echo $LC_ALL
locale -a | grep -i utf

# 3. Si el locale no está instalado:
sudo locale-gen "$LANG"

# 4. Regenerar todos los locales:
sudo dpkg-reconfigure locales   # Debian
```

```bash
# PROBLEMA: sort/order devuelve resultados inesperados

# Causa: LC_COLLATE diferente

# Solución: forzar C locale en la operación:
LC_ALL=C sort archivo.txt
```

---

## Quick reference

```
VER LOCALE:
  locale              → todas las variables
  locale charmap     → codificación
  locale -a         → locales instalados

CAMBiar:
  Debian:  sudo update-locale LANG=es_ES.UTF-8
  RHEL:    sudo localectl set-locale LANG=es_ES.UTF-8

GENERAR LOCALE:
  sudo locale-gen es_ES.UTF-8

EN SCRIPTS:
  LC_ALL=C sort archivo.txt           → orden byte a byte
  LC_ALL=C grep '[a-z]' archivo      → regex predecible
  LC_ALL=C awk '{print $1}'        → números predecibles

UTF-8:
  file archivo.txt                    → ver codificación
  iconv -f ISO-8859-1 -t UTF-8 archivo.txt → convertir
```

---

## Ejercicios

### Ejercicio 1 — Locale actual

```bash
# 1. ¿Cuál es tu LANG?
echo "LANG=$LANG"

# 2. ¿Codificación?
locale charmap

# 3. ¿LC_* diferentes de LANG?
locale | grep -v "LANG\|LC_ALL" | awk -F= '{if($2!="\"\"" && $2!="\"" ENVIRON["LANG"] "\"") print}'

# 4. ¿Cuántos locales instalados?
locale -a | wc -l
```

### Ejercicio 2 — Efecto del locale en date

```bash
# Comparar formato de fecha:
echo "=== C ==="
LC_ALL=C date +"%x"

echo "=== en_US ==="
LC_ALL=en_US.UTF-8 date +"%x"

echo "=== es_ES ==="
LC_ALL=es_ES.UTF-8 date +"%x" 2>/dev/null || echo "No disponible"

echo "=== de_DE ==="
LC_ALL=de_DE.UTF-8 date +"%x" 2>/dev/null || echo "No disponible"
```

### Ejercicio 3 — Sort y locale

```bash
# Datos de prueba:
DATA="banana
Árbol
apple
Ñoño
zebra"

echo "=== LC_ALL=C ==="
echo "$DATA" | LC_ALL=C sort

echo "=== LC_ALL=en_US.UTF-8 ==="
echo "$DATA" | LC_ALL=en_US.UTF-8 sort 2>/dev/null || echo "No disponible"

echo "=== LC_ALL=es_ES.UTF-8 ==="
echo "$DATA" | LC_ALL=es_ES.UTF-8 sort 2>/dev/null || echo "No disponible"
```

### Ejercicio 4 — Scripts robustos

```bash
# Crear script que ordena correctamente:

cat << 'EOF' > /usr/local/bin/safe-sort.sh
#!/bin/bash
# Ordena archivos de forma predecible

# Usar LC_ALL=C para ordenamiento byte a byte
export LC_ALL=C

if [ -t 0 ]; then
    # Entrada interactiva
    echo "Escribe líneas para ordenar (Ctrl+D para terminar):"
    sort
else
    # Entrada desde pipe/archivo
    sort "$@"
fi
EOF
chmod +x /usr/local/bin/safe-sort.sh

# Probar:
echo -e "banana\nÁrbol\napple" | /usr/local/bin/safe-sort.sh
```

### Ejercicio 5 — Conversión de codificación

```bash
# 1. Ver la codificación de un archivo de texto:
file /var/log/syslog

# 2. Si no está en UTF-8, convertir:
# iconv -f ISO-8859-1 -t UTF-8 archivo.txt -o archivo-utf8.txt

# 3. Verificar que quedó en UTF-8:
# file archivo-utf8.txt
```
