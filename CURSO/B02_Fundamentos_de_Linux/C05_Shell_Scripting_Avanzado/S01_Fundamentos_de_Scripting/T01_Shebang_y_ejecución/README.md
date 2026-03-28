# T01 — Shebang y ejecución

## Qué es el shebang

El **shebang** (también hashbang) es la primera línea de un script que indica
al kernel qué intérprete debe usarse para ejecutarlo:

```bash
#!/bin/bash
echo "Hola desde bash"
```

```
#!  → "magic number" que el kernel reconoce
/bin/bash → ruta al intérprete

Cuando ejecutas: ./script.sh
El kernel lee los primeros bytes, ve #! y ejecuta:
  /bin/bash ./script.sh
```

Sin shebang, el kernel no sabe qué intérprete usar. Si se ejecuta con
`./script.sh`, el shell actual intenta interpretarlo (puede funcionar o no).
Con `bash script.sh`, se fuerza bash — pero el shebang es la forma correcta
y explícita.

## #!/bin/bash vs #!/usr/bin/env bash

### Ruta absoluta

```bash
#!/bin/bash
```

Ventaja: directo, sin ambigüedad.
Problema: asume que bash está en `/bin/bash`. En algunos sistemas (FreeBSD,
NixOS, contenedores mínimos), bash puede estar en otro lugar.

### env lookup

```bash
#!/usr/bin/env bash
```

`env` busca `bash` en el `PATH` del usuario y ejecuta el primero que
encuentra. Es más portable:

```
#!/usr/bin/env bash
  → env busca bash en PATH
  → Encuentra /bin/bash (o /usr/local/bin/bash, o donde esté)
  → Ejecuta ese bash con el script

Ventaja: funciona sin importar dónde esté instalado bash
Desventaja: depende de lo que haya en PATH (si hay un bash
            diferente antes en PATH, ejecutará ese)
```

### Cuándo usar cada uno

| Situación | Shebang |
|---|---|
| Scripts de sistema (init, cron) | `#!/bin/bash` (ruta conocida y fija) |
| Scripts portables (múltiples distros) | `#!/usr/bin/env bash` |
| Scripts de Python | `#!/usr/bin/env python3` (python puede estar en muchos sitios) |
| Scripts POSIX (máxima portabilidad) | `#!/bin/sh` |

### sh vs bash

```bash
#!/bin/sh
# sh es POSIX shell — funcionalidad mínima garantizada
# En Debian: sh → dash (no bash)
# En RHEL: sh → bash

# Si usas features de bash (arrays, [[ ]], ${var//pat/rep}),
# el script FALLARÁ con #!/bin/sh en Debian porque dash no las soporta

ls -la /bin/sh
# Debian: lrwxrwxrwx 1 root root 4 ... /bin/sh -> dash
# RHEL:   lrwxrwxrwx 1 root root 4 ... /bin/sh -> bash
```

Regla: si usas cualquier feature de bash (y lo harás), usar `#!/bin/bash` o
`#!/usr/bin/env bash`. Usar `#!/bin/sh` solo para scripts POSIX puros.

## Permisos de ejecución

Un script necesita el bit de ejecución (`x`) para ejecutarse directamente:

```bash
# Crear un script
cat > script.sh << 'EOF'
#!/bin/bash
echo "Funciona"
EOF

# Sin permiso de ejecución:
./script.sh
# bash: ./script.sh: Permission denied

# Dar permiso de ejecución
chmod +x script.sh

# Ahora funciona
./script.sh
# Funciona

# Alternativa: ejecutar explícitamente con el intérprete (no necesita +x)
bash script.sh
# Funciona (no necesita chmod +x)
```

### Permisos típicos para scripts

```bash
chmod 755 script.sh    # rwxr-xr-x — todos pueden ejecutar
chmod 700 script.sh    # rwx------ — solo el owner
chmod 750 script.sh    # rwxr-x--- — owner y grupo
```

## Ejecución: ./script vs script vs source

### ./script.sh — Subshell

```bash
./script.sh
# 1. El kernel lee el shebang
# 2. Crea un nuevo proceso (fork + exec)
# 3. El script ejecuta en un SUBSHELL (proceso hijo)
# 4. Las variables del script NO afectan al shell padre

# Ejemplo:
cat > test.sh << 'EOF'
#!/bin/bash
MY_VAR="desde el script"
cd /tmp
EOF
chmod +x test.sh

./test.sh
echo $MY_VAR    # (vacío — la variable murió con el subshell)
pwd              # (sigue en el directorio original)
```

### source script.sh (o . script.sh)

```bash
source script.sh
# o equivalente POSIX:
. script.sh

# Ejecuta el script en el shell ACTUAL (no crea subshell)
# Las variables y cambios de directorio PERSISTEN

source test.sh
echo $MY_VAR    # "desde el script"
pwd              # /tmp (el cd afectó al shell actual)
```

`source` se usa para:
- Cargar configuración: `source ~/.bashrc`
- Cargar funciones de librería: `source /usr/lib/mi-lib.sh`
- Activar entornos: `source venv/bin/activate`

### bash script.sh — Sin shebang

```bash
bash script.sh
# Ejecuta el script con bash explícitamente
# No necesita permiso de ejecución
# No necesita shebang (se ignora)
# Crea un subshell (como ./)
```

### Resumen

| Forma | Subshell | Necesita +x | Usa shebang | Variables persisten |
|---|---|---|---|---|
| `./script.sh` | Sí | Sí | Sí | No |
| `bash script.sh` | Sí | No | No (se ignora) | No |
| `source script.sh` | No | No | No (se ignora) | Sí |

## PATH y localización de scripts

Cuando ejecutas un comando sin ruta (`script.sh` en lugar de `./script.sh`),
el shell lo busca en los directorios listados en `PATH`:

```bash
echo $PATH
# /usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# El shell busca en orden:
# 1. /usr/local/bin/script.sh
# 2. /usr/bin/script.sh
# 3. /bin/script.sh
# 4. ... no encontrado → "command not found"
```

### Hacer un script accesible globalmente

```bash
# Opción 1: copiar a un directorio en PATH
sudo cp script.sh /usr/local/bin/script
sudo chmod 755 /usr/local/bin/script

# Opción 2: agregar tu directorio al PATH
mkdir -p ~/bin
cp script.sh ~/bin/
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Opción 3: symlink
sudo ln -s /path/to/script.sh /usr/local/bin/script
```

### Por qué ./script.sh y no script.sh

```bash
# El directorio actual (.) NO está en PATH por defecto
# (por seguridad — evita que un atacante ponga un "ls" malicioso
# en un directorio y tú lo ejecutes sin querer)

script.sh
# bash: script.sh: command not found

./script.sh
# Funciona — ./ indica explícitamente "en el directorio actual"

# NUNCA agregar . al PATH:
# export PATH=".:$PATH"    ← PELIGROSO
```

## Shebang con opciones

```bash
#!/bin/bash -e
# Equivale a: bash -e script.sh
# -e = salir si cualquier comando falla (set -e)

#!/bin/bash -eu
# -e = salir si falla un comando
# -u = error si se usa una variable no definida

#!/usr/bin/env -S bash -eu
# Con env, las opciones se pasan con -S (split string)
# No todos los sistemas soportan env -S (GNU coreutils 8.30+)
```

En la práctica, es más claro poner las opciones dentro del script:

```bash
#!/usr/bin/env bash
set -euo pipefail
# Más portable que ponerlas en el shebang
```

## Shebangs para otros lenguajes

```bash
#!/usr/bin/env python3
#!/usr/bin/env perl
#!/usr/bin/env ruby
#!/usr/bin/env node
#!/usr/bin/awk -f
#!/usr/bin/sed -f
```

El shebang es una feature del kernel de Linux, no de bash. Funciona con
cualquier intérprete.

---

## Ejercicios

### Ejercicio 1 — Shebang y ejecución

```bash
# Crear un script
cat > /tmp/test-shebang.sh << 'EOF'
#!/bin/bash
echo "Shell: $BASH"
echo "PID: $$"
echo "Parent PID: $PPID"
EOF
chmod +x /tmp/test-shebang.sh

# Ejecutar de tres formas
echo "=== ./script ==="
/tmp/test-shebang.sh

echo "=== bash script ==="
bash /tmp/test-shebang.sh

echo "=== source script ==="
source /tmp/test-shebang.sh

# ¿Cuál tiene PID diferente? ¿Cuál comparte PID con tu shell?

rm /tmp/test-shebang.sh
```

### Ejercicio 2 — source vs subshell

```bash
cat > /tmp/test-source.sh << 'EOF'
#!/bin/bash
TEST_VAR="valor_del_script"
EOF

# Con subshell
bash /tmp/test-source.sh
echo "Subshell: TEST_VAR=$TEST_VAR"

# Con source
source /tmp/test-source.sh
echo "Source: TEST_VAR=$TEST_VAR"

unset TEST_VAR
rm /tmp/test-source.sh
```

### Ejercicio 3 — sh vs bash

```bash
# ¿A qué apunta /bin/sh en tu sistema?
ls -la /bin/sh

# Crear un script con feature de bash
cat > /tmp/test-sh.sh << 'EOF'
#!/bin/sh
arr=(uno dos tres)
echo "${arr[1]}"
EOF
chmod +x /tmp/test-sh.sh

# ¿Funciona con sh?
/tmp/test-sh.sh

# Cambiar a bash
sed -i '1s|.*|#!/bin/bash|' /tmp/test-sh.sh
/tmp/test-sh.sh

rm /tmp/test-sh.sh
```
