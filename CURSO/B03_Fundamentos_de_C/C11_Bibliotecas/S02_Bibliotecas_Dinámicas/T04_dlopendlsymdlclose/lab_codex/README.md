# Lab — dlopen dlsym dlclose

## Objetivo

Construir un host que carga plugins en runtime, obtiene la fabrica
`plugin_create` con `dlsym`, ejecuta todos los plugins encontrados en un
directorio y los descarga correctamente al final.

## Prerequisitos

- GCC instalado
- `nm`, `ldd` y `file` disponibles
- Estar en el directorio `lab_codex/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `plugin_api.h` | Interfaz comun para todos los plugins |
| `host.c` | Host que recorre `plugins/` y carga `.so` |
| `plugins_src/plugin_upper.c` | Plugin de mayusculas |
| `plugins_src/plugin_reverse.c` | Plugin de inversion |
| `Makefile` | Build del host y plugins |

---

## Parte 1 — Revisar la interfaz y el host

**Objetivo**: Entender el contrato entre host y plugin.

### Paso 1.1 — Examinar la interfaz comun

```bash
cat plugin_api.h
```

Observa:

- Cada plugin expone una estructura `Plugin`
- La funcion obligatoria es `plugin_create(void)`
- El host solo necesita conocer esa interfaz, no el codigo del plugin

### Paso 1.2 — Examinar el host

```bash
cat host.c
```

Busca estas piezas:

- `dlopen(path, RTLD_NOW)`
- `dlsym(handle, "plugin_create")`
- llamada a `plugin->init()`
- llamada a `plugin->process(...)`
- `dlclose(handle)` al final

Antes de continuar, responde mentalmente:

- Que ocurre si aparece un plugin nuevo en `plugins/` pero no recompilas `host`?
- En que momento se valida que `plugin_create` realmente exista?

---

## Parte 2 — Construir el host y un plugin

**Objetivo**: Cargar un primer plugin con `dlopen`.

### Paso 2.1 — Compilar el host

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 host.c -ldl -o host
mkdir -p plugins
```

### Paso 2.2 — Compilar el primer plugin

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -shared \
    plugins_src/plugin_upper.c -o plugins/plugin_upper.so
```

### Paso 2.3 — Verificar el simbolo exportado

```bash
nm -D plugins/plugin_upper.so | grep plugin_create
```

Debes ver un simbolo `T plugin_create`.

### Paso 2.4 — Ejecutar el host

```bash
./host
```

Salida esperada:

```text
[upper] init
loaded plugin: upper v1.0
[upper] LINUX C RUST
[upper] cleanup
```

---

## Parte 3 — Agregar un segundo plugin sin recompilar el host

**Objetivo**: Demostrar el valor real de `dlopen`.

### Paso 3.1 — Compilar un plugin nuevo

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -fPIC -shared \
    plugins_src/plugin_reverse.c -o plugins/plugin_reverse.so
```

### Paso 3.2 — Ejecutar el MISMO host

```bash
./host
```

Salida esperada:

```text
[reverse] init
loaded plugin: reverse v1.0
[upper] init
loaded plugin: upper v1.0
[reverse] tsuR C xuniL
[upper] LINUX C RUST
[reverse] cleanup
[upper] cleanup
```

Lo importante es que `host` no se recompilo. Solo se agrego un `.so` nuevo.

### Paso 3.3 — Confirmar dependencias del host

```bash
ldd host
```

Observa que `host` no depende de `plugin_upper.so` ni de `plugin_reverse.so`.
Los plugins aparecen solo en runtime por medio de `dlopen`.

---

## Parte 4 — Repetir el flujo con `make`

**Objetivo**: Automatizar la construccion y limpieza.

### Paso 4.1 — Examinar el Makefile

```bash
cat Makefile
```

### Paso 4.2 — Build completo

```bash
make clean
make host
make plugin_upper
./host

make plugin_reverse
./host
```

### Paso 4.3 — Limpiar

```bash
make clean
```

---

## Limpieza final

```bash
make clean
rm -f host
rm -rf plugins
```

---

## Conceptos reforzados

1. `dlopen` carga una biblioteca en runtime y devuelve un handle opaco.
2. `dlsym` obtiene un simbolo por nombre y requiere un cast al tipo correcto.
3. `plugin_create` funciona como punto de entrada estable entre host y plugin.
4. El host no necesita recompilarse para descubrir plugins nuevos.
5. `dlclose` libera el handle cuando ya no se necesita.
6. `nm -D` permite comprobar rapidamente si un plugin exporta el simbolo
   esperado.
