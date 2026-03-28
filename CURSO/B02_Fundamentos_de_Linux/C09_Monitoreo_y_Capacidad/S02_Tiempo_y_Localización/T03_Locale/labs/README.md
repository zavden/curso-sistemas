# Lab — Locale

## Objetivo

Entender el sistema de locales en Linux: formato de nombre
(idioma_TERRITORIO.CODIFICACION), variables LC_* (CTYPE, NUMERIC,
TIME, COLLATE, MONETARY, MESSAGES), precedencia (LC_ALL > LC_* >
LANG), locale C/POSIX (predecible, ASCII), configuracion del sistema
(Debian: /etc/default/locale, locale-gen; RHEL: /etc/locale.conf,
localectl), implicaciones en scripts (sort, grep, separador
decimal), y UTF-8.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Variables LC_* y precedencia

### Objetivo

Entender las variables de locale y su precedencia.

### Paso 1.1: Locale actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Locale actual ==="
echo ""
locale
echo ""
echo "--- Formato del nombre ---"
echo "  idioma_TERRITORIO.CODIFICACION"
echo "  es_ES.UTF-8  = espanol de Espana, UTF-8"
echo "  en_US.UTF-8  = ingles de EEUU, UTF-8"
echo "  pt_BR.UTF-8  = portugues de Brasil, UTF-8"
echo ""
echo "El territorio importa:"
echo "  es_ES → moneda: EUR"
echo "  es_MX → moneda: MXN"
echo "  en_US → fecha: 03/17/2026"
echo "  en_GB → fecha: 17/03/2026"
'
```

### Paso 1.2: Variables LC_*

```bash
docker compose exec debian-dev bash -c '
echo "=== Variables LC_* ==="
echo ""
echo "Cada aspecto del locale se controla con una variable:"
echo ""
echo "  LANG          valor por defecto (catch-all)"
echo "  LC_CTYPE      clasificacion de caracteres"
echo "  LC_NUMERIC    formato de numeros (. vs ,)"
echo "  LC_TIME       formato de fechas y horas"
echo "  LC_COLLATE    ordenamiento de texto"
echo "  LC_MONETARY   formato de moneda"
echo "  LC_MESSAGES   idioma de mensajes del sistema"
echo "  LC_PAPER      tamano de papel (A4, Letter)"
echo "  LC_NAME       formato de nombres de personas"
echo "  LC_ADDRESS    formato de direcciones"
echo "  LC_TELEPHONE  formato de telefonos"
echo "  LC_MEASUREMENT sistema de medidas"
echo "  LC_ALL        sobreescribe TODO"
echo ""
echo "--- Efecto de LC_TIME ---"
echo "  C:      $(LC_ALL=C date +"%A")"
echo "  en_US:  $(LC_ALL=en_US.UTF-8 date +"%A" 2>/dev/null || echo "(no disponible)")"
echo "  es_ES:  $(LC_ALL=es_ES.UTF-8 date +"%A" 2>/dev/null || echo "(no disponible)")"
echo ""
echo "--- Efecto de LC_MESSAGES ---"
echo "  C:      $(LC_ALL=C ls /noexiste 2>&1)"
echo "  es_ES:  $(LC_ALL=es_ES.UTF-8 ls /noexiste 2>&1 || echo "(locale no disponible)")"
'
```

### Paso 1.3: Precedencia

```bash
docker compose exec debian-dev bash -c '
echo "=== Precedencia ==="
echo ""
echo "De mayor a menor prioridad:"
echo ""
echo "  1. LC_ALL     → sobreescribe TODO (nuclear)"
echo "  2. LC_*       → cada variable individual"
echo "  3. LANG       → valor por defecto"
echo ""
echo "--- Ejemplo ---"
echo "  LANG=es_ES.UTF-8        default: espanol"
echo "  LC_TIME=en_US.UTF-8     pero fechas en ingles"
echo "  LC_ALL=                  no sobreescribir"
echo ""
echo "  Si LC_ALL esta definido, ignora LANG y todas las LC_*"
echo "  Usarla solo para forzar locale en contexto especifico:"
echo "  LC_ALL=C sort archivo.txt"
echo ""
echo "--- Estado actual ---"
echo "  LANG=$(locale | grep ^LANG= | cut -d= -f2)"
echo "  LC_ALL=$(locale | grep ^LC_ALL= | cut -d= -f2)"
echo ""
LANG_VAL=$(locale | grep "^LANG=" | cut -d= -f2 | tr -d "\"")
DIFF=$(locale | grep -v "^LANG=\|^LC_ALL=" | grep -v "\"$LANG_VAL\"" | grep -v "=$LANG_VAL" | grep -cv "^$" 2>/dev/null || echo 0)
echo "  Variables LC_* diferentes de LANG: $DIFF"
'
```

### Paso 1.4: Locale C / POSIX

```bash
docker compose exec debian-dev bash -c '
echo "=== Locale C / POSIX ==="
echo ""
echo "C (o POSIX) es el locale minimo y predecible:"
echo "  - ASCII puro"
echo "  - Mensajes en ingles"
echo "  - Ordenamiento byte a byte"
echo "  - Sin formato regional"
echo ""
echo "C.UTF-8 — como C pero con soporte UTF-8"
echo ""
echo "--- Cuando usar C ---"
echo ""
echo "1. En scripts — para salida predecible:"
echo "   LC_ALL=C sort archivo.txt"
echo ""
echo "2. Parsear salida de comandos:"
echo "   LC_ALL=C df -h | awk ..."
echo "   (. como separador decimal, no ,)"
echo ""
echo "3. Expresiones regulares predecibles:"
echo "   LC_ALL=C grep \"[a-z]\" archivo"
echo "   (solo a-z ASCII, no a, n, u)"
echo ""
echo "--- Locales disponibles ---"
locale -a 2>/dev/null | head -10
echo "..."
echo "Total: $(locale -a 2>/dev/null | wc -l) locales"
'
```

---

## Parte 2 — Configuracion del sistema

### Objetivo

Configurar locales en Debian y RHEL.

### Paso 2.1: Debian

```bash
docker compose exec debian-dev bash -c '
echo "=== Configuracion en Debian ==="
echo ""
echo "--- /etc/default/locale ---"
if [[ -f /etc/default/locale ]]; then
    cat /etc/default/locale
else
    echo "  (no encontrado)"
fi
echo ""
echo "--- Locales generados ---"
echo "Los locales se generan (compilan) con locale-gen:"
locale -a 2>/dev/null | head -10
echo ""
echo "--- /etc/locale.gen ---"
echo "Lista de locales a generar (descomentando lineas):"
if [[ -f /etc/locale.gen ]]; then
    grep -v "^#" /etc/locale.gen | grep -v "^$" | head -5
    echo "..."
    echo "Total activos: $(grep -v "^#" /etc/locale.gen | grep -v "^$" | wc -l)"
else
    echo "  (no encontrado)"
fi
echo ""
echo "--- Comandos ---"
echo "  locale-gen es_ES.UTF-8        generar un locale"
echo "  dpkg-reconfigure locales      menu interactivo"
echo "  update-locale LANG=es_ES.UTF-8  cambiar default"
'
```

### Paso 2.2: RHEL

```bash
docker compose exec alma-dev bash -c '
echo "=== Configuracion en RHEL ==="
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
echo "--- Comandos ---"
echo "  localectl set-locale LANG=es_ES.UTF-8"
echo "  localectl list-locales | grep es"
echo ""
echo "--- Instalar locales ---"
echo "  dnf install glibc-langpack-es    (espanol)"
echo "  dnf install glibc-all-langpacks  (todos)"
echo ""
echo "--- Paquetes de idioma instalados ---"
rpm -qa glibc-langpack* 2>/dev/null | head -5 || echo "  (verificar con rpm)"
'
```

### Paso 2.3: Comparacion

```bash
docker compose exec debian-dev bash -c '
echo "=== Debian vs RHEL ==="
echo ""
echo "| Aspecto | Debian | RHEL |"
echo "|---------|--------|------|"
echo "| Config | /etc/default/locale | /etc/locale.conf |"
echo "| Generar | locale-gen, locale.gen | glibc-langpack-* |"
echo "| Cambiar | update-locale | localectl set-locale |"
echo "| Menu | dpkg-reconfigure locales | (no hay) |"
echo "| Listar | locale -a | localectl list-locales |"
'
```

---

## Parte 3 — Implicaciones en scripts

### Objetivo

Entender como el locale afecta el comportamiento de comandos.

### Paso 3.1: sort y locale

```bash
docker compose exec debian-dev bash -c '
echo "=== sort y locale ==="
echo ""
echo "sort se comporta diferente segun el locale."
echo ""
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
echo "--- Diferencia ---"
echo "  C: ordena por valor de byte (A-Z antes que a-z)"
echo "  en_US: ordena linguisticamente (a y A juntos)"
echo ""
echo "REGLA: en scripts, usar LC_ALL=C para ordenamiento predecible."
'
```

### Paso 3.2: grep y clases de caracteres

```bash
docker compose exec debian-dev bash -c '
echo "=== grep y locale ==="
echo ""
echo "[a-z] NO es lo mismo en todos los locales:"
echo ""
echo "--- En C ---"
echo "cafe" | LC_ALL=C grep -o "[a-z]*"
echo "  (solo a-z ASCII)"
echo ""
echo "--- Caracteres especiales ---"
echo "En C, [a-z] NO incluye a, n, u, e..."
echo "En locales con soporte UTF-8, SI puede incluirlos."
echo ""
echo "--- Alternativas seguras ---"
echo "  [[:alpha:]]  cualquier letra (respeta locale)"
echo "  [[:lower:]]  minusculas (respeta locale)"
echo "  [[:upper:]]  mayusculas (respeta locale)"
echo "  [[:digit:]]  digitos"
echo ""
echo "  Para ASCII estricto: usar LC_ALL=C"
'
```

### Paso 3.3: Separador decimal

```bash
docker compose exec debian-dev bash -c '
echo "=== Separador decimal ==="
echo ""
echo "Algunos locales usan coma como separador decimal:"
echo ""
echo "--- LC_NUMERIC=C ---"
echo "  3.14"
echo ""
echo "--- LC_NUMERIC=de_DE.UTF-8 ---"
echo "  3,14 (si el locale esta disponible)"
echo ""
echo "--- En scripts que parsean numeros ---"
echo "SIEMPRE usar LC_ALL=C o LC_NUMERIC=C"
echo ""
echo "Ejemplo seguro:"
echo "  VALUE=\$(LC_ALL=C awk \"{print \\\$1 * 1.5}\" <<< \"3.14\")"
VALUE=$(LC_ALL=C awk "{print \$1 * 1.5}" <<< "3.14")
echo "  Resultado: $VALUE"
echo ""
echo "Si LC_NUMERIC no es C, el . podria no parsearse"
echo "como separador decimal."
'
```

### Paso 3.4: Buenas practicas para scripts

```bash
docker compose exec debian-dev bash -c '
echo "=== Buenas practicas ==="
echo ""
echo "1. Para scripts que parsean salida de comandos:"
echo "   export LC_ALL=C"
echo "   (todo en ASCII, comportamiento predecible)"
echo ""
echo "2. Para scripts que interactuan con usuarios:"
echo "   Respetar el locale del sistema"
echo "   Pero usar LC_ALL=C para operaciones internas"
echo ""
echo "3. Patron seguro:"
cat << '\''EOF'\''
#!/bin/bash
# Guardar locale del usuario
USER_LOCALE="$LC_ALL"

# Operaciones internas en C:
export LC_ALL=C
RESULT=$(some_command | grep pattern | awk '{print $3}')

# Restaurar para mensajes al usuario:
export LC_ALL="$USER_LOCALE"
echo "Resultado: $RESULT"
EOF
echo ""
echo "4. Para parsear numeros: LC_NUMERIC=C"
echo "5. Para ordenar: LC_COLLATE=C"
echo "6. Para regex: LC_CTYPE=C (o LC_ALL=C)"
'
```

### Paso 3.5: UTF-8

```bash
docker compose exec debian-dev bash -c '
echo "=== UTF-8 ==="
echo ""
echo "UTF-8 es la codificacion estandar en Linux moderno."
echo "Soporta todos los caracteres Unicode."
echo ""
echo "--- Verificar ---"
echo "  Codificacion actual: $(locale charmap 2>/dev/null || echo "desconocida")"
echo ""
echo "--- Si no es UTF-8 ---"
echo "  update-locale LANG=en_US.UTF-8    (Debian)"
echo "  localectl set-locale LANG=en_US.UTF-8  (RHEL)"
echo ""
echo "--- Convertir archivos ---"
echo "  file archivo.txt                  ver codificacion"
echo "  iconv -f ISO-8859-1 -t UTF-8 in > out  convertir"
echo ""
echo "--- Tabla resumen ---"
echo ""
echo "| Variable | Afecta a | Cuidado con |"
echo "|----------|----------|-------------|"
echo "| LC_COLLATE | sort, ls, glob | Ordenamiento inesperado |"
echo "| LC_CTYPE | regex, tr, wc | [a-z] incluye acentos? |"
echo "| LC_NUMERIC | printf, awk | . vs , como decimal |"
echo "| LC_MESSAGES | mensajes de error | Parsear errores |"
echo "| LC_TIME | date | Formato de fecha |"
echo "| LC_ALL | TODO | Sobreescribe todo |"
'
```

---

## Limpieza final

No hay recursos que limpiar.

---

## Conceptos reforzados

1. Un locale define convenciones regionales: idioma,
   fechas, numeros, moneda, ordenamiento, codificacion.
   Formato: idioma_TERRITORIO.CODIFICACION.

2. Precedencia: LC_ALL > LC_* individuales > LANG.
   LC_ALL sobreescribe todo, usarla solo para forzar.

3. Locale C/POSIX: minimo, predecible, ASCII.
   Usar en scripts para comportamiento consistente.

4. sort, grep [a-z], y separador decimal cambian segun
   el locale. En scripts: LC_ALL=C para operaciones
   internas, respetar locale para mensajes al usuario.

5. Debian: /etc/default/locale + locale-gen.
   RHEL: /etc/locale.conf + glibc-langpack-*.

6. UTF-8 es el estandar. Verificar con locale charmap.
   Convertir con iconv si es necesario.
