# Lab — Verificacion

## Objetivo

Entender, ejecutar y utilizar el script de verificacion del entorno del
laboratorio. Aprender a interpretar la salida, provocar fallos
intencionalmente, y diagnosticar problemas comunes.

## Prerequisitos

- Docker y Docker Compose instalados
- Entorno del curso levantado (T03 compose.yml corriendo)
- Estar en el directorio `labs/` de este topico

## Archivos del laboratorio

| Archivo | Proposito |
|---|---|
| `verify-env.sh` | Script de verificacion del entorno |

---

## Parte 1 — Examinar el script

### Objetivo

Entender la estructura del script antes de ejecutarlo.

### Paso 1.1: Ver el script

```bash
cat verify-env.sh
```

### Paso 1.2: Funciones de verificacion

El script define dos funciones:

```bash
grep -A 8 "^check()" verify-env.sh
```

`check()` ejecuta un comando y reporta OK/FAIL segun el exit code.

```bash
grep -A 8 "^check_output()" verify-env.sh
```

`check_output()` ademas captura la salida del comando y la muestra
junto al OK (util para ver versiones).

### Paso 1.3: Secciones de verificacion

```bash
grep "^echo \"---" verify-env.sh
```

El script verifica en orden:
1. Docker instalado y activo
2. Docker Compose instalado
3. Contenedores corriendo
4. Herramientas de compilacion (ambas distros)
5. Herramientas de debugging (ambas distros)
6. Rust (ambas distros)
7. Compilacion real de C y Rust
8. gdb con SYS_PTRACE
9. Volumenes compartidos

### Paso 1.4: Flag -T en exec

```bash
grep "exec -T" verify-env.sh | head -3
```

El flag `-T` desactiva la asignacion de pseudo-TTY. Es necesario cuando
`exec` se usa dentro de un script (no interactivo). Sin `-T`, los
comandos fallan porque no hay terminal disponible.

### Paso 1.5: set -euo pipefail

```bash
head -4 verify-env.sh
```

- `set -e` — sale del script si un comando falla
- `set -u` — trata variables no definidas como error
- `set -o pipefail` — un fallo en un pipeline reporta el error

Este es el patron estandar para scripts bash robustos.

---

## Parte 2 — Ejecutar la verificacion

### Objetivo

Ejecutar el script de verificacion contra el entorno del curso y
aprender a interpretar la salida.

### Paso 2.1: Preparar

El script necesita ejecutarse desde el directorio donde esta el
compose.yml del curso. Si el entorno del T03 esta levantado, copia
el script alli:

```bash
cp verify-env.sh /path/to/T03/labs/verify-env.sh
cd /path/to/T03/labs/
```

Si no tienes el entorno del T03 levantado, levantalo primero:

```bash
cd /path/to/T03/labs/
docker compose up -d
```

### Paso 2.2: Dar permisos de ejecucion

```bash
chmod +x verify-env.sh
```

### Paso 2.3: Ejecutar

```bash
./verify-env.sh
```

Antes de ejecutar, predice:

- Cuantos checks pasaran?
- Habra algun FAIL?

Salida esperada (si todo esta correctamente configurado):

```
============================================
  Verificacion del entorno de laboratorio
============================================

--- Docker ---
  [OK]   Docker instalado
  [OK]   Docker daemon activo

--- Docker Compose ---
  [OK]   Docker Compose instalado
  [OK]   Servicios definidos

--- Contenedores ---
  [OK]   debian-dev corriendo
  [OK]   alma-dev corriendo

--- Debian: Compilacion ---
  [OK]   gcc -> gcc (Debian 12.x.x) 12.x.x
  [OK]   g++ -> g++ (Debian 12.x.x) 12.x.x
  ...

============================================
  Resultados: ~28 OK, 0 FAIL
============================================

  Entorno verificado correctamente.
```

### Paso 2.4: Verificar el exit code

```bash
./verify-env.sh
echo "Exit code: $?"
```

Si todas las verificaciones pasan, el exit code es 0. Si alguna falla,
el exit code es 1. Esto permite usar el script en pipelines:

```bash
./verify-env.sh && echo "Entorno listo" || echo "Hay problemas"
```

---

## Parte 3 — Provocar fallos

### Objetivo

Provocar fallos intencionalmente para practicar el diagnostico de
problemas comunes.

### Paso 3.1: Fallo de SYS_PTRACE

Para simular la falta de SYS_PTRACE, edita temporalmente el
compose.yml y comenta `cap_add`:

```bash
# Guardar backup
cp compose.yml compose.yml.bak

# Comentar cap_add (editar manualmente o con sed)
sed -i 's/^    cap_add:/    #cap_add:/' compose.yml
sed -i 's/^      - SYS_PTRACE/      #- SYS_PTRACE/' compose.yml
```

Recrear los contenedores y verificar:

```bash
docker compose down
docker compose up -d
./verify-env.sh
```

Predice: que checks fallaran?

Los checks de gdb (`--- SYS_PTRACE (gdb) ---`) deberian fallar con
`[FAIL]`. Las demas herramientas siguen funcionando.

Restaurar:

```bash
cp compose.yml.bak compose.yml
docker compose down
docker compose up -d
```

### Paso 3.2: Fallo de contenedor detenido

```bash
docker compose stop alma-dev
./verify-env.sh
```

Predice: que checks fallaran?

Todos los checks de AlmaLinux fallan, ademas de
"alma-dev corriendo" y "Volumen compartido".

Restaurar:

```bash
docker compose start alma-dev
```

### Paso 3.3: Verificar despues de restaurar

```bash
./verify-env.sh
```

Todos los checks deben pasar de nuevo. La verificacion confirma que
el entorno se recupero correctamente.

---

## Limpieza final

No hay recursos adicionales que limpiar. Si deseas detener el entorno
del curso:

```bash
cd /path/to/T03/labs/
docker compose down -v
```

---

## Conceptos reforzados

1. El script de verificacion usa `check()` y `check_output()` para
   ejecutar comandos y reportar OK/FAIL de forma estructurada.

2. El flag `-T` es necesario cuando `docker compose exec` se usa
   dentro de scripts (sin terminal interactiva).

3. `set -euo pipefail` es el patron estandar para scripts bash
   robustos: falla rapido ante errores, variables no definidas, y
   fallos en pipelines.

4. El exit code del script (0 = OK, 1 = FAIL) permite integrarlo en
   pipelines y automatizacion.

5. Los fallos mas comunes son: falta de **SYS_PTRACE** (gdb falla),
   **contenedor detenido** (todos los checks de esa distro fallan),
   y **volumen no compartido** (verificacion de datos falla).
