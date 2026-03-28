# Lab — Shebang y ejecucion

## Objetivo

Entender el shebang (#!/bin/bash vs #!/usr/bin/env bash), por que
sh no es lo mismo que bash en Debian (sh→dash), la necesidad de
chmod +x, las diferencias entre ./script, source y bash script, y
como PATH afecta la resolucion de interpretes.

## Prerequisitos

- Entorno del curso levantado (`docker compose up -d`)

---

## Parte 1 — Shebang

### Objetivo

Entender que es el shebang, como el kernel lo usa para elegir el
interprete, y la diferencia entre rutas absolutas y env.

### Paso 1.1: El shebang en accion

```bash
docker compose exec debian-dev bash -c '
echo "=== Que es el shebang ==="
echo ""
echo "La primera linea de un script ejecutable:"
echo "  #!/bin/bash"
echo ""
echo "El kernel lee los 2 primeros bytes del archivo (#!)"
echo "y usa el resto de la linea como interprete"
echo ""
echo "--- Crear un script con shebang bash ---"
cat > /tmp/hello.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Interprete: $BASH_VERSION"
echo "Shell: $0"
echo "PID: $$"
SCRIPT
chmod +x /tmp/hello.sh
echo ""
echo "--- Contenido ---"
cat /tmp/hello.sh
echo ""
echo "--- Ejecutar ---"
/tmp/hello.sh
'
```

### Paso 1.2: sh no es bash en Debian

```bash
docker compose exec debian-dev bash -c '
echo "=== sh en Debian = dash ==="
echo ""
echo "--- Que es /bin/sh ---"
ls -la /bin/sh
echo ""
echo "En Debian, /bin/sh es un symlink a dash (no a bash)"
echo "dash es un shell POSIX minimo, mas rapido pero menos funciones"
echo ""
echo "--- Crear un script con #!/bin/sh ---"
cat > /tmp/test-sh.sh << '\''SCRIPT'\''
#!/bin/sh
echo "Variable BASH_VERSION: ${BASH_VERSION:-no definida}"
echo "Proceso: $(readlink -f /proc/$$/exe 2>/dev/null || echo desconocido)"
SCRIPT
chmod +x /tmp/test-sh.sh
echo ""
echo "--- Ejecutar con #!/bin/sh ---"
/tmp/test-sh.sh
echo ""
echo "BASH_VERSION no existe en dash porque no es bash"
'
```

### Paso 1.3: Bashismos que fallan en sh

Antes de ejecutar, predice: que pasara con `[[ ]]` y arrays
bajo #!/bin/sh (dash)?

```bash
docker compose exec debian-dev bash -c '
echo "=== Bashismos en dash ==="
echo ""
echo "--- Script con bashismos ---"
cat > /tmp/bashism.sh << '\''SCRIPT'\''
#!/bin/sh
# Intentar usar [[ ]] (bashismo)
NAME="test"
if [[ "$NAME" == "test" ]]; then
    echo "Funciona"
fi
SCRIPT
chmod +x /tmp/bashism.sh
echo ""
cat /tmp/bashism.sh
echo ""
echo "--- Ejecutar (fallara) ---"
/tmp/bashism.sh 2>&1 || true
echo ""
echo "[[ ]] no existe en dash (POSIX sh)"
echo "En dash se debe usar: [ \"$NAME\" = \"test\" ]"
echo ""
echo "=== Otros bashismos que fallan en dash ==="
echo "  [[ ]]          → usar [ ]"
echo "  arrays         → no existen en POSIX sh"
echo "  \${var,,}       → no existe"
echo "  \${var//pat/rep} → no existe"
echo "  (( ))          → usar \$((  )) o [ \$x -eq 1 ]"
echo "  <<<            → no existe"
echo "  select         → no existe"
'
```

### Paso 1.4: env vs ruta absoluta

```bash
docker compose exec debian-dev bash -c '
echo "=== #!/usr/bin/env bash vs #!/bin/bash ==="
echo ""
echo "--- Ruta absoluta ---"
echo "#!/bin/bash"
echo "  Busca bash EXACTAMENTE en /bin/bash"
echo "  Si bash esta en otro lugar, falla"
echo ""
echo "--- env ---"
echo "#!/usr/bin/env bash"
echo "  Busca bash en el PATH del usuario"
echo "  Mas portable (funciona en mas sistemas)"
echo ""
echo "--- Donde esta bash ---"
which bash
echo ""
echo "--- Donde esta env ---"
which env
echo ""
echo "--- Sistemas donde importa ---"
echo "  Linux:   bash en /bin/bash (siempre)"
echo "  FreeBSD: bash en /usr/local/bin/bash"
echo "  NixOS:   bash en /run/current-system/sw/bin/bash"
echo "  macOS:   /bin/bash es bash 3.2, env encuentra bash 5 de brew"
echo ""
echo "Recomendacion: #!/usr/bin/env bash para portabilidad"
echo "Excepcion: scripts de sistema que requieren ruta fija"
'
```

---

## Parte 2 — Formas de ejecucion

### Objetivo

Entender las diferencias entre ./script, bash script, y source script,
y cuando usar cada una.

### Paso 2.1: chmod +x y ejecucion directa

```bash
docker compose exec debian-dev bash -c '
echo "=== chmod +x ==="
echo ""
echo "--- Crear script sin permiso de ejecucion ---"
cat > /tmp/no-exec.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Ejecutado correctamente"
SCRIPT
echo ""
echo "--- Permisos actuales ---"
ls -l /tmp/no-exec.sh
echo ""
echo "--- Intentar ejecutar sin +x ---"
/tmp/no-exec.sh 2>&1 || true
echo ""
echo "--- Agregar permiso de ejecucion ---"
chmod +x /tmp/no-exec.sh
ls -l /tmp/no-exec.sh
echo ""
echo "--- Ejecutar con +x ---"
/tmp/no-exec.sh
'
```

### Paso 2.2: bash script.sh vs ./script.sh

```bash
docker compose exec debian-dev bash -c '
echo "=== bash script.sh vs ./script.sh ==="
echo ""
echo "--- Crear script SIN shebang y SIN +x ---"
cat > /tmp/no-shebang.sh << '\''SCRIPT'\''
echo "BASH_VERSION=$BASH_VERSION"
echo "Shebang: (ninguno)"
SCRIPT
echo ""
echo "--- bash script.sh (ignora shebang y permisos) ---"
bash /tmp/no-shebang.sh
echo ""
echo "--- ./script.sh (necesita shebang y +x) ---"
chmod +x /tmp/no-shebang.sh
/tmp/no-shebang.sh 2>&1 || true
echo ""
echo "Sin shebang, el kernel no sabe que interprete usar"
echo "Algunos shells asumen sh, otros fallan"
echo ""
echo "bash script.sh:"
echo "  - NO necesita shebang (fuerza bash)"
echo "  - NO necesita chmod +x"
echo "  - Util para pruebas rapidas"
echo ""
echo "./script.sh:"
echo "  - NECESITA shebang (el kernel lo lee)"
echo "  - NECESITA chmod +x"
echo "  - Es la forma correcta para scripts finales"
'
```

### Paso 2.3: source vs ejecucion

Antes de ejecutar, predice: si un script hace `cd /tmp` y
`export VAR=hello`, que pasa en el shell actual despues de
ejecutarlo con ./script vs source?

```bash
docker compose exec debian-dev bash -c '
echo "=== source vs ./script ==="
echo ""
echo "--- Crear script que modifica el entorno ---"
cat > /tmp/change-env.sh << '\''SCRIPT'\''
#!/bin/bash
export LAB_VAR="desde_el_script"
cd /tmp
echo "Dentro del script: PWD=$PWD LAB_VAR=$LAB_VAR"
SCRIPT
chmod +x /tmp/change-env.sh
echo ""
echo "=== Ejecutar con ./ (subshell) ==="
unset LAB_VAR 2>/dev/null || true
cd /root
echo "Antes: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
/tmp/change-env.sh
echo "Despues: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
echo ""
echo "→ El subshell cambio cd y export, pero el shell actual NO cambio"
echo ""
echo "=== Ejecutar con source (mismo shell) ==="
unset LAB_VAR 2>/dev/null || true
cd /root
echo "Antes: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
source /tmp/change-env.sh
echo "Despues: PWD=$PWD LAB_VAR=${LAB_VAR:-no_definida}"
echo ""
echo "→ source ejecuta en el MISMO shell: cd y export persisten"
'
```

### Paso 2.4: Cuando usar cada forma

```bash
docker compose exec debian-dev bash -c '
echo "=== Resumen de formas de ejecucion ==="
echo ""
printf "%-22s %-12s %-12s %-12s\n" "Forma" "Subshell" "Necesita +x" "Necesita #!"
printf "%-22s %-12s %-12s %-12s\n" "---------------------" "-----------" "-----------" "-----------"
printf "%-22s %-12s %-12s %-12s\n" "./script.sh" "Si" "Si" "Si"
printf "%-22s %-12s %-12s %-12s\n" "bash script.sh" "Si" "No" "No"
printf "%-22s %-12s %-12s %-12s\n" "source script.sh" "No" "No" "No"
printf "%-22s %-12s %-12s %-12s\n" ". script.sh" "No" "No" "No"
echo ""
echo "Usos tipicos:"
echo "  ./script.sh         → scripts finales, automatizacion"
echo "  bash script.sh      → pruebas rapidas, debugging"
echo "  source script.sh    → cargar funciones/variables al shell actual"
echo "  . script.sh         → equivalente POSIX de source"
echo ""
echo "source se usa para:"
echo "  . ~/.bashrc          → recargar configuracion"
echo "  source venv/bin/activate → activar virtualenv Python"
echo "  . .env               → cargar variables de entorno"
'
```

---

## Parte 3 — PATH y seguridad

### Objetivo

Entender como PATH afecta la resolucion de interpretes y comandos,
y las implicaciones de seguridad.

### Paso 3.1: PATH y el shebang

```bash
docker compose exec debian-dev bash -c '
echo "=== PATH y el shebang ==="
echo ""
echo "--- PATH actual ---"
echo "$PATH" | tr ":" "\n" | head -8
echo ""
echo "--- Resolucion con env ---"
echo "#!/usr/bin/env bash busca bash en el PATH"
echo ""
echo "Si alguien modifica PATH:"
echo "  export PATH=/tmp/malicioso:\$PATH"
echo "  Y pone un bash falso en /tmp/malicioso/bash"
echo "  #!/usr/bin/env bash ejecutaria el falso"
echo ""
echo "#!/bin/bash siempre ejecuta /bin/bash"
echo "(no se ve afectado por PATH)"
echo ""
echo "--- En la practica ---"
echo "Para scripts de usuario: #!/usr/bin/env bash (portabilidad)"
echo "Para scripts de sistema (/etc/init.d, cron root): #!/bin/bash (seguridad)"
'
```

### Paso 3.2: Ejecutar scripts del directorio actual

```bash
docker compose exec debian-dev bash -c '
echo "=== Por que ./script.sh y no script.sh ==="
echo ""
echo "--- Intentar sin ./ ---"
cat > /tmp/test-path.sh << '\''SCRIPT'\''
#!/bin/bash
echo "Ejecutado"
SCRIPT
chmod +x /tmp/test-path.sh
cd /tmp
test-path.sh 2>&1 || true
echo ""
echo "El shell busca comandos en PATH, no en el directorio actual"
echo "El directorio actual (.) NO esta en PATH por seguridad"
echo ""
echo "--- Verificar ---"
echo "$PATH" | tr ":" "\n" | grep "^\.$" || echo "\".\" NO esta en PATH (correcto)"
echo ""
echo "--- Por que es importante ---"
echo "Si . estuviera en PATH, un atacante podria poner un ls"
echo "malicioso en un directorio y esperar a que alguien lo ejecute"
echo ""
echo "Siempre usar ./ para scripts del directorio actual"
'
```

### Paso 3.3: Verificar el interprete real

```bash
docker compose exec debian-dev bash -c '
echo "=== Verificar que interprete se usa ==="
echo ""
echo "--- Shebang de scripts del sistema ---"
head -1 /etc/init.d/* 2>/dev/null | grep "#!" | head -5 || echo "(sin scripts init.d)"
echo ""
echo "--- Rutas de interpretes comunes ---"
printf "%-25s %s\n" "Interprete" "Ruta"
printf "%-25s %s\n" "------------------------" "----------------------------"
for cmd in bash sh dash python3 perl; do
    path=$(which "$cmd" 2>/dev/null || echo "no encontrado")
    printf "%-25s %s\n" "$cmd" "$path"
done
echo ""
echo "--- file: verificar que tipo de archivo es ---"
file /bin/bash
file /bin/sh
echo ""
echo "--- readlink: resolver symlinks ---"
readlink -f /bin/sh
'
```

---

## Limpieza final

```bash
docker compose exec debian-dev bash -c '
rm -f /tmp/hello.sh /tmp/test-sh.sh /tmp/bashism.sh
rm -f /tmp/no-exec.sh /tmp/no-shebang.sh /tmp/change-env.sh
rm -f /tmp/test-path.sh
echo "Archivos temporales eliminados"
'
```

---

## Conceptos reforzados

1. El **shebang** (`#!`) en la primera linea indica al kernel que
   interprete usar. Sin shebang, `./script.sh` puede fallar o
   usar un interprete inesperado.

2. En Debian, `/bin/sh` es un symlink a **dash**, no a bash.
   Scripts con bashismos (`[[ ]]`, arrays, `<<<`) fallan bajo
   `#!/bin/sh`.

3. `#!/usr/bin/env bash` busca bash en el PATH del usuario,
   siendo mas portable. `#!/bin/bash` es mas seguro para scripts
   de sistema porque no depende de PATH.

4. `./script.sh` ejecuta en un **subshell** (necesita `#!` y `+x`).
   `source script.sh` ejecuta en el **shell actual** — los cambios
   de `cd`, `export`, y funciones persisten.

5. El directorio actual (`.`) **no esta en PATH** por seguridad.
   Siempre usar `./script.sh` para ejecutar scripts locales.

6. `bash script.sh` fuerza el interprete y no necesita permisos
   ni shebang — util para pruebas rapidas.
