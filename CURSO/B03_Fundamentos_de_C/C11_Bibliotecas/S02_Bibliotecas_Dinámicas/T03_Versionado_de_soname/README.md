# T03 — Versionado de soname

## El problema del versionado

Cuando una biblioteca dinámica se actualiza, los programas
que dependen de ella pueden romperse si la nueva versión
cambia la interfaz (API o ABI):

```
Escenario:
1. Compilas program.c con libfoo.so versión 1.0
2. libfoo.so se actualiza a versión 2.0 (cambia la API)
3. Tu programa falla al ejecutar → "undefined symbol" o crash

Linux resuelve esto con un sistema de tres nombres.
```

## Los tres nombres

```
Real name:    libvec3.so.1.2.3    ← archivo físico en disco
Soname:       libvec3.so.1        ← symlink, identifica compatibilidad ABI
Linker name:  libvec3.so          ← symlink, lo usa gcc al compilar

libvec3.so → libvec3.so.1 → libvec3.so.1.2.3
  (linker)     (soname)        (real name)
```

```bash
# Ejemplo real en el sistema:
ls -la /usr/lib/x86_64-linux-gnu/libz*
# libz.so         → libz.so.1.2.13     (linker name — symlink)
# libz.so.1       → libz.so.1.2.13     (soname — symlink)
# libz.so.1.2.13                        (real name — archivo)
```

### Real name (nombre real)

```bash
# El archivo físico con TODO el código.
# Nombre: lib<nombre>.so.<major>.<minor>.<patch>

# libvec3.so.1.2.3
#              │ │ │
#              │ │ └─ patch: bug fixes (compatible)
#              │ └─── minor: nueva funcionalidad (compatible)
#              └───── major: cambio de ABI (INCOMPATIBLE)

# Cada versión nueva es un archivo diferente:
# libvec3.so.1.0.0
# libvec3.so.1.1.0   ← nueva función, compatible con 1.0
# libvec3.so.1.2.3   ← bug fix, compatible con 1.0
# libvec3.so.2.0.0   ← cambio de ABI, INCOMPATIBLE con 1.x
```

### Soname (nombre compartido)

```bash
# Symlink que apunta al real name.
# Solo incluye el major version.
# Nombre: lib<nombre>.so.<major>

# libvec3.so.1 → libvec3.so.1.2.3

# El soname se graba DENTRO del .so al compilarlo:
readelf -d libvec3.so.1.2.3 | grep SONAME
# (SONAME)  Library soname: [libvec3.so.1]

# Y se graba en el ejecutable que depende de él:
readelf -d program | grep NEEDED
# (NEEDED)  Shared library: [libvec3.so.1]

# En runtime, ld-linux.so busca "libvec3.so.1" (el soname),
# NO "libvec3.so.1.2.3" (el real name).
# El symlink libvec3.so.1 → libvec3.so.1.2.3 hace la conexión.
```

```bash
# ¿Por qué?
# Porque se puede actualizar de 1.2.3 a 1.3.0 sin recompilar:
#
# Antes:
# libvec3.so.1 → libvec3.so.1.2.3
#
# Después de actualizar:
# libvec3.so.1 → libvec3.so.1.3.0   ← symlink actualizado
# libvec3.so.1.2.3                    ← se puede mantener o borrar
#
# El programa busca "libvec3.so.1" → encuentra la nueva versión.
# Como el major no cambió → compatible → funciona.
```

### Linker name (nombre de enlace)

```bash
# Symlink sin versión, usado solo en compilación.
# Nombre: lib<nombre>.so

# libvec3.so → libvec3.so.1

# gcc -lvec3 busca "libvec3.so" en los directorios de búsqueda.
# El symlink redirige a la versión correcta.

# Este symlink lo crea:
# - El paquete -dev (libfoo-dev en Debian/Ubuntu)
# - ldconfig (automáticamente)
# - O manualmente: ln -s libvec3.so.1 libvec3.so
```

## Crear una biblioteca con soname

```bash
# 1. Compilar con -fPIC:
gcc -c -fPIC -Wall -O2 vec3.c -o vec3.o

# 2. Crear el .so con soname embebido:
gcc -shared -Wl,-soname,libvec3.so.1 -o libvec3.so.1.2.3 vec3.o

# -Wl,-soname,libvec3.so.1:
# -Wl,  → pasar el siguiente argumento al linker
# -soname,libvec3.so.1 → grabar "libvec3.so.1" como soname

# 3. Crear los symlinks:
ln -sf libvec3.so.1.2.3 libvec3.so.1    # soname → real
ln -sf libvec3.so.1 libvec3.so           # linker → soname

# 4. Resultado:
ls -la libvec3*
# libvec3.so       → libvec3.so.1        (linker name)
# libvec3.so.1     → libvec3.so.1.2.3    (soname)
# libvec3.so.1.2.3                        (real name)
```

```bash
# Verificar el soname:
readelf -d libvec3.so.1.2.3 | grep SONAME
# (SONAME)  Library soname: [libvec3.so.1]

objdump -p libvec3.so.1.2.3 | grep SONAME
# SONAME  libvec3.so.1

# Compilar un programa:
gcc main.c -L. -lvec3 -lm -o program

# Verificar qué busca el programa:
readelf -d program | grep NEEDED
# (NEEDED)  Shared library: [libvec3.so.1]
# Busca por soname, no por real name ni linker name.
```

## Instalar y actualizar

```bash
# Instalar la biblioteca:
sudo cp libvec3.so.1.2.3 /usr/local/lib/
sudo ldconfig

# ldconfig automáticamente:
# 1. Crea libvec3.so.1 → libvec3.so.1.2.3 (soname symlink)
# 2. Actualiza /etc/ld.so.cache

# Crear el linker name manualmente (o con el paquete -dev):
sudo ln -sf libvec3.so.1 /usr/local/lib/libvec3.so

# Verificar:
ldconfig -p | grep vec3
# libvec3.so.1 (libc6,x86-64) => /usr/local/lib/libvec3.so.1.2.3
```

```bash
# ACTUALIZACIÓN COMPATIBLE (minor/patch):
# libvec3 1.2.3 → 1.3.0

sudo cp libvec3.so.1.3.0 /usr/local/lib/
sudo ldconfig
# ldconfig actualiza: libvec3.so.1 → libvec3.so.1.3.0
# Todos los programas que buscan libvec3.so.1 usan la nueva versión.
# Sin recompilar nada.
```

```bash
# ACTUALIZACIÓN INCOMPATIBLE (major):
# libvec3 1.x → 2.0.0

sudo cp libvec3.so.2.0.0 /usr/local/lib/
sudo ldconfig
# Crea: libvec3.so.2 → libvec3.so.2.0.0 (nuevo soname)
# Mantiene: libvec3.so.1 → libvec3.so.1.3.0 (soname anterior)

# Los programas compilados con v1 buscan libvec3.so.1 → siguen funcionando.
# Los programas nuevos compilados con v2 buscan libvec3.so.2.
# AMBAS versiones coexisten.

# Actualizar el linker name para nuevas compilaciones:
sudo ln -sf libvec3.so.2 /usr/local/lib/libvec3.so
# Ahora gcc -lvec3 enlaza con v2.
```

## Coexistencia de versiones

```bash
# En el sistema pueden coexistir:
/usr/local/lib/
├── libvec3.so           → libvec3.so.2       (linker: usa v2 para compilar)
├── libvec3.so.1         → libvec3.so.1.3.0   (soname v1)
├── libvec3.so.1.3.0                            (real name v1)
├── libvec3.so.2         → libvec3.so.2.0.0   (soname v2)
└── libvec3.so.2.0.0                            (real name v2)

# Programa A (compilado con v1):
ldd programa_a
# libvec3.so.1 => /usr/local/lib/libvec3.so.1.3.0

# Programa B (compilado con v2):
ldd programa_b
# libvec3.so.2 => /usr/local/lib/libvec3.so.2.0.0

# Ambos funcionan simultáneamente, cada uno con su versión.
```

## Cuándo cambiar el major version

```c
// Cambio INCOMPATIBLE (major++) — rompe ABI:
// - Eliminar una función pública
// - Cambiar los parámetros o retorno de una función
// - Cambiar el layout de un struct público (agregar/quitar/reordenar campos)
// - Cambiar el tamaño de un tipo público
// - Cambiar el significado de una constante o enum

// Cambio COMPATIBLE (minor++) — extiende ABI:
// - Agregar una función nueva
// - Agregar un campo AL FINAL de un struct (si se usa puntero)
// - Agregar un nuevo valor de enum

// Bug fix (patch++):
// - Corregir un bug sin cambiar la interfaz
// - Optimización interna
```

## Semantic versioning resumido

```
MAJOR.MINOR.PATCH

MAJOR: cambio incompatible de API/ABI
MINOR: nueva funcionalidad, compatible hacia atrás
PATCH: bug fix, compatible hacia atrás

En bibliotecas compartidas Linux:
- soname = lib<name>.so.<MAJOR>
- real name = lib<name>.so.<MAJOR>.<MINOR>.<PATCH>
```

---

## Ejercicios

### Ejercicio 1 — Tres nombres

```bash
# 1. Crear libcalc.so.1.0.0 con soname libcalc.so.1
#    con funciones add(int, int) y mul(int, int).
# 2. Crear los symlinks correctos.
# 3. Compilar main.c con -lcalc.
# 4. Verificar con readelf -d que el programa busca libcalc.so.1.
# 5. Verificar con readelf -d que el .so tiene SONAME = libcalc.so.1.
```

### Ejercicio 2 — Actualización compatible

```bash
# 1. Con libcalc.so.1.0.0 del ejercicio anterior, compilar un programa.
# 2. Crear libcalc.so.1.1.0 que agrega la función sub(int, int).
# 3. Actualizar el symlink libcalc.so.1 → libcalc.so.1.1.0.
# 4. Ejecutar el programa SIN recompilar.
#    ¿Funciona? ¿Usa la versión nueva?
# 5. Verificar con ldd.
```

### Ejercicio 3 — Coexistencia de versiones

```bash
# 1. Crear libcalc.so.1.0.0 con add(int, int) → retorna a + b.
# 2. Crear libcalc.so.2.0.0 con add(int, int, int) → retorna a + b + c.
#    (API incompatible → major 2).
# 3. Instalar ambas versiones.
# 4. Compilar programa_v1 con v1 y programa_v2 con v2.
# 5. Verificar que ambos funcionan simultáneamente.
# 6. ¿Qué pasa si ejecutas programa_v1 después de borrar libcalc.so.1?
```
