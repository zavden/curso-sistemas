# T03 — Locale

## Qué es un locale

Un locale define las **convenciones regionales** del sistema: idioma,
formato de fechas, números, moneda, ordenamiento de texto y codificación
de caracteres:

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

### Formato de nombre

```bash
# idioma_TERRITORIO.CODIFICACIÓN
# es_ES.UTF-8  → español de España, UTF-8
# en_US.UTF-8  → inglés de Estados Unidos, UTF-8
# pt_BR.UTF-8  → portugués de Brasil, UTF-8
# de_DE.UTF-8  → alemán de Alemania, UTF-8

# El territorio importa:
# es_ES → formato de fecha: 17/03/2026, moneda: €
# es_MX → formato de fecha: 17/03/2026, moneda: $
# en_US → formato de fecha: 03/17/2026, moneda: $
# en_GB → formato de fecha: 17/03/2026, moneda: £
```

## Variables LC_*

Cada aspecto del locale se controla con una variable diferente:

```bash
# LANG — valor por defecto para todas las LC_* no definidas
# Es la variable "catch-all"

# LC_CTYPE — clasificación de caracteres
# Qué es una letra, qué es un dígito, mayúsculas/minúsculas
# Afecta a: regex, tr, wc, funciones isalpha()/toupper()

# LC_NUMERIC — formato de números
# Separador decimal (. vs ,), separador de miles
echo "1234567.89" | LC_NUMERIC=de_DE.UTF-8 numfmt --grouping
# 1.234.567,89

# LC_TIME — formato de fechas y horas
LC_TIME=es_ES.UTF-8 date +"%A %d de %B de %Y"
# martes 17 de marzo de 2026
LC_TIME=en_US.UTF-8 date +"%A %d de %B de %Y"
# Tuesday 17 of March of 2026

# LC_COLLATE — ordenamiento de texto
# Cómo se ordenan las letras (á después de a o después de z?)
# Afecta a: sort, ls, glob patterns

# LC_MONETARY — formato de moneda
# Símbolo, posición, decimales

# LC_MESSAGES — idioma de los mensajes del sistema
# Los mensajes de error, help, etc.
LC_MESSAGES=es_ES.UTF-8 ls /noexiste
# ls: no se puede acceder a '/noexiste': No existe el archivo o el directorio
LC_MESSAGES=en_US.UTF-8 ls /noexiste
# ls: cannot access '/noexiste': No such file or directory

# LC_PAPER — tamaño de papel (A4, Letter)
# LC_NAME — formato de nombres de personas
# LC_ADDRESS — formato de direcciones
# LC_TELEPHONE — formato de teléfonos
# LC_MEASUREMENT — sistema de medidas (métrico vs imperial)
# LC_IDENTIFICATION — metadatos del locale
```

### Precedencia

```bash
# La precedencia de variables (de mayor a menor):
# 1. LC_ALL     — sobreescribe TODO (nuclear)
# 2. LC_*       — cada variable individual
# 3. LANG       — valor por defecto

# Ejemplo:
LANG=es_ES.UTF-8       # default: español
LC_TIME=en_US.UTF-8    # pero las fechas en inglés
LC_ALL=                 # no sobreescribir nada

# LC_ALL es la variable "override total":
# Si LC_ALL está definido, ignora LANG y todas las LC_* individuales
# Usarla solo para forzar un locale en un contexto específico:
LC_ALL=C sort archivo.txt
```

## LANG, LC_ALL y C/POSIX

### LANG

```bash
# LANG define el locale por defecto del sistema:

# Ver:
echo $LANG
# en_US.UTF-8

# Cambiar permanentemente:
# Debian/Ubuntu:
sudo update-locale LANG=es_ES.UTF-8
# Modifica /etc/default/locale

# RHEL/Fedora:
sudo localectl set-locale LANG=es_ES.UTF-8
# Modifica /etc/locale.conf

# El cambio aplica en la PRÓXIMA sesión (re-login)
```

### El locale C / POSIX

```bash
# C (o POSIX) es el locale mínimo y predecible:
# - ASCII puro (sin caracteres especiales)
# - Mensajes en inglés
# - Ordenamiento byte a byte (no lingüístico)
# - Sin formato regional

# C.UTF-8 — como C pero con soporte UTF-8:
# - Caracteres UTF-8 válidos
# - Pero sin convenciones regionales

# Cuándo usar C / POSIX:
# 1. En scripts — para salida predecible:
LC_ALL=C sort archivo.txt
# Ordena byte a byte, sin sorpresas por locale

# 2. Parsear salida de comandos:
LC_ALL=C df -h | awk '{print $5}'
# Siempre usa . como separador decimal, no ,

# 3. Expresiones regulares predecibles:
LC_ALL=C grep '[a-z]' archivo
# Solo a-z ASCII, no incluye á, ñ, ü

# IMPORTANTE: con locale es_ES.UTF-8:
echo "Ángel" | grep '[A-Z]'
# Puede coincidir o no, dependiendo de LC_COLLATE
echo "Ángel" | LC_ALL=C grep '[A-Z]'
# Solo coincide con 'A' (byte 0x41), Á es multibyte
```

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

sudo locale-gen    # regenerar

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

# En RHEL, los locales se instalan con el paquete glibc-langpack:
sudo dnf install glibc-langpack-es    # español
sudo dnf install glibc-all-langpacks  # todos
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

# Si un locale no aparece, hay que generarlo/instalarlo
```

## Implicaciones en scripts

### Problema: sort con locale

```bash
# sort se comporta diferente según el locale:

# Con locale C:
echo -e "banana\nÁrbol\napple" | LC_ALL=C sort
# apple
# banana
# Árbol         ← Á tiene valor byte mayor que letras minúsculas

# Con locale es_ES:
echo -e "banana\nÁrbol\napple" | LC_ALL=es_ES.UTF-8 sort
# apple
# Árbol         ← Á se ordena con la A (lingüísticamente correcto)
# banana

# Regla: en scripts, usar LC_ALL=C para ordenamiento predecible
```

### Problema: grep con clases de caracteres

```bash
# [a-z] no es lo mismo en todos los locales:

# En C/POSIX:
echo "café" | LC_ALL=C grep -o '[a-z]*'
# caf         ← é no es [a-z] en ASCII

# En es_ES:
echo "café" | LC_ALL=es_ES.UTF-8 grep -o '[a-z]*'
# café        ← é es una letra minúscula en español

# Alternativa segura — usar clases POSIX:
echo "café" | grep -oP '[\p{Ll}]+'    # PCRE: lowercase letter (Unicode)
echo "café" | grep -o '[[:alpha:]]*'   # POSIX: letras (respeta locale)
```

### Problema: separador decimal

```bash
# Algunos locales usan coma como separador decimal:
LC_NUMERIC=de_DE.UTF-8 printf "%'.2f\n" 1234567.89
# 1.234.567,89

LC_NUMERIC=en_US.UTF-8 printf "%'.2f\n" 1234567.89
# 1,234,567.89

# En scripts que parsean números:
# SIEMPRE usar LC_NUMERIC=C para que . sea el decimal
VALUE=$(LC_ALL=C awk '{print $1 * 1.5}' <<< "3.14")
```

### Buenas prácticas para scripts

```bash
# 1. Para scripts que parsean salida de comandos:
export LC_ALL=C
# Todo en ASCII, comportamiento predecible

# 2. Para scripts que interactúan con usuarios:
# Respetar el locale del sistema (no forzar LC_ALL)
# Pero usar LC_ALL=C para operaciones internas

# 3. Patrón seguro:
#!/bin/bash
# Guardar locale del usuario
USER_LOCALE="$LC_ALL"

# Operaciones internas en C:
export LC_ALL=C
RESULT=$(some_command | grep pattern | awk '{print $3}')

# Restaurar para mensajes al usuario:
export LC_ALL="$USER_LOCALE"
echo "Resultado: $RESULT"
```

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

# Archivos en otra codificación:
file archivo.txt
# archivo.txt: ISO-8859-1 text

# Convertir a UTF-8:
iconv -f ISO-8859-1 -t UTF-8 archivo.txt > archivo-utf8.txt
```

---

## Ejercicios

### Ejercicio 1 — Locale actual

```bash
# 1. ¿Cuál es tu LANG?
echo "LANG=$LANG"

# 2. ¿Hay algún LC_* diferente de LANG?
locale | grep -v "LANG\|LC_ALL" | while IFS='=' read -r var val; do
    if [ "$val" != "\"$LANG\"" ] && [ "$val" != "$LANG" ]; then
        echo "Diferente: $var=$val"
    fi
done

# 3. ¿Cuántos locales están instalados?
locale -a | wc -l
```

### Ejercicio 2 — Efecto del locale

```bash
# Comparar el efecto del locale en date:
echo "=== C ==="
LC_ALL=C date
echo "=== es_ES ==="
LC_ALL=es_ES.UTF-8 date 2>/dev/null || echo "Locale no disponible"
echo "=== en_US ==="
LC_ALL=en_US.UTF-8 date 2>/dev/null || echo "Locale no disponible"
```

### Ejercicio 3 — Sort y locale

```bash
# Comparar ordenamiento:
DATA="banana
Ángel
apple
Ñoño
zebra"

echo "=== LC_ALL=C ==="
echo "$DATA" | LC_ALL=C sort
echo "=== LC_ALL=en_US.UTF-8 ==="
echo "$DATA" | LC_ALL=en_US.UTF-8 sort
```
