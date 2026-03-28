# T02 — Enlace dinámico

## Cómo funciona el enlace dinámico

A diferencia del enlace estático (que copia código), el enlace
dinámico deja **referencias** que se resuelven cuando el programa
se ejecuta:

```
Compilación (link time):
┌──────────┐     ┌──────────────┐
│ main.o   │     │ libvec3.so   │
│ call vec3 │────→│ vec3_create  │  El linker verifica que los
│   [PLT]  │     │ vec3_add     │  símbolos EXISTEN, pero no
└──────────┘     └──────────────┘  copia el código.
      │
      ▼
┌──────────┐
│ programa │  Contiene: PLT stubs + referencia a "libvec3.so"
│ 20 KB    │  NO contiene: el código de vec3_create
└──────────┘

Ejecución (runtime):
┌──────────┐     ┌──────────────┐
│ programa │     │ libvec3.so   │  ld-linux.so carga libvec3.so
│ call vec3 │────→│ vec3_create  │  en memoria y resuelve las
│ via PLT  │     │              │  direcciones en la GOT.
└──────────┘     └──────────────┘
```

## El dynamic linker: ld-linux.so

```bash
# ld-linux.so es el programa que carga bibliotecas dinámicas.
# Es lo primero que se ejecuta cuando lanzas un programa dinámico.

file /usr/bin/ls
# ELF 64-bit LSB pie executable, x86-64, ...
# interpreter /lib64/ld-linux-x86-64.so.2

# El "interpreter" es ld-linux.so — el kernel lo ejecuta primero.
# ld-linux.so:
# 1. Lee el binario y encuentra qué .so necesita
# 2. Busca cada .so en los paths configurados
# 3. Carga los .so en memoria (mmap)
# 4. Resuelve los símbolos (llena la GOT)
# 5. Llama a main()

# Ejecutar ld-linux.so directamente:
/lib64/ld-linux-x86-64.so.2 --list /usr/bin/ls
# Equivale a: ldd /usr/bin/ls
```

## PLT y GOT

```c
// PLT (Procedure Linkage Table):
// Tabla de "stubs" — funciones pequeñas que redireccionan
// a la dirección real de la función.

// GOT (Global Offset Table):
// Tabla de punteros que contiene las direcciones reales
// de las funciones y variables de las .so cargadas.

// Primera llamada a vec3_create():
// 1. main() → call PLT[vec3_create]
// 2. PLT stub lee GOT[vec3_create]
// 3. GOT todavía no tiene la dirección → llama a ld-linux.so
// 4. ld-linux.so busca vec3_create en libvec3.so
// 5. Escribe la dirección real en GOT[vec3_create]
// 6. Salta a vec3_create()

// Segunda llamada (y todas las siguientes):
// 1. main() → call PLT[vec3_create]
// 2. PLT stub lee GOT[vec3_create]
// 3. GOT ya tiene la dirección → salta directamente
// → Sin overhead de resolución (lazy binding)
```

```bash
# Ver la PLT y GOT de un binario:
objdump -d program | grep "@plt"
# 0000000000401030 <vec3_create@plt>:
# 0000000000401040 <vec3_add@plt>:
# 0000000000401050 <printf@plt>:

readelf -r program | grep GLOB_DAT
# Muestra entradas de la GOT
```

## Dónde busca las bibliotecas dinámicas

```bash
# El linker dinámico busca .so en este orden:
#
# 1. RPATH del binario (embebido en el ELF, deprecated)
# 2. LD_LIBRARY_PATH (variable de entorno)
# 3. RUNPATH del binario (embebido en el ELF)
# 4. Cache de ldconfig (/etc/ld.so.cache)
# 5. Directorios por defecto: /lib, /usr/lib
#    (y variantes como /lib/x86_64-linux-gnu)
```

### LD_LIBRARY_PATH

```bash
# Variable de entorno con directorios de búsqueda adicionales:
export LD_LIBRARY_PATH=/opt/mylibs/lib:$LD_LIBRARY_PATH
./program

# O para un solo comando:
LD_LIBRARY_PATH=./lib ./program

# Uso: desarrollo local, pruebas.
# NO usar en producción — es frágil y afecta TODOS los programas.
# Problemas:
# - Puede cargar .so incorrectos
# - No se hereda en setuid/setgid (seguridad)
# - Difícil de mantener
```

### ldconfig

```bash
# ldconfig gestiona la cache de bibliotecas dinámicas.

# Agregar un directorio de bibliotecas:
echo "/opt/mylibs/lib" | sudo tee /etc/ld.so.conf.d/mylibs.conf
sudo ldconfig

# ldconfig:
# 1. Lee /etc/ld.so.conf (y /etc/ld.so.conf.d/*.conf)
# 2. Escanea los directorios listados
# 3. Crea symlinks de soname (libfoo.so.1 → libfoo.so.1.2.3)
# 4. Actualiza el cache /etc/ld.so.cache

# Ver el cache:
ldconfig -p | grep vec3
# libvec3.so.1 (libc6,x86-64) => /usr/local/lib/libvec3.so.1.0.0

# Instalar una biblioteca al sistema:
sudo cp libvec3.so.1.0.0 /usr/local/lib/
sudo ldconfig
# Ahora cualquier programa puede encontrar libvec3.so.
```

### RPATH y RUNPATH

```bash
# Embeber el path de búsqueda dentro del binario:

# RUNPATH (recomendado):
gcc main.c -L./lib -lvec3 -Wl,-rpath,'$ORIGIN/lib' -o program
# $ORIGIN = directorio del ejecutable.
# El programa buscará libvec3.so en ./lib/ relativo a sí mismo.
# Funciona sin LD_LIBRARY_PATH ni ldconfig.

# Verificar:
readelf -d program | grep RUNPATH
# (RUNPATH)  Library runpath: [$ORIGIN/lib]

# Ejemplos de RUNPATH:
-Wl,-rpath,/opt/myapp/lib          # path absoluto
-Wl,-rpath,'$ORIGIN/lib'           # relativo al ejecutable
-Wl,-rpath,'$ORIGIN/../lib'        # un nivel arriba

# Escapar $ORIGIN en shell:
# Usar comillas simples: '$ORIGIN/lib'
# O escapar: \$ORIGIN/lib
```

```bash
# RPATH vs RUNPATH:
# RPATH (legacy): se busca ANTES de LD_LIBRARY_PATH
#   → el usuario no puede overridear con LD_LIBRARY_PATH
# RUNPATH: se busca DESPUÉS de LD_LIBRARY_PATH
#   → el usuario puede overridear (más flexible)
#
# gcc moderno genera RUNPATH por defecto.
# Para forzar RPATH: -Wl,--disable-new-dtags
# Para forzar RUNPATH: -Wl,--enable-new-dtags (default)
```

## Diagnóstico de problemas

```bash
# 1. "error while loading shared libraries: libfoo.so: cannot open"
# La biblioteca no está en ningún path de búsqueda.
# Diagnóstico:
ldd program
# libfoo.so => not found

# Soluciones (en orden de preferencia):
# a) Instalar la biblioteca: sudo apt install libfoo-dev
# b) ldconfig: sudo ldconfig (si ya está instalada pero no en cache)
# c) RUNPATH: recompilar con -Wl,-rpath
# d) LD_LIBRARY_PATH: solo para desarrollo/testing

# 2. "symbol lookup error: undefined symbol: func_name"
# La biblioteca se encontró pero no tiene el símbolo esperado.
# Causa: versión incorrecta de la biblioteca.
nm -D /path/to/libfoo.so | grep func_name
```

```bash
# Ver qué bibliotecas carga un programa (en runtime):
LD_DEBUG=libs ./program 2>&1 | head -30
# Muestra cada paso de búsqueda.

# Ver la resolución de símbolos:
LD_DEBUG=symbols ./program 2>&1 | head -50

# Ver TODO:
LD_DEBUG=all ./program 2>&1 | less

# LD_DEBUG es muy útil para diagnosticar problemas de carga.
```

## Preloading con LD_PRELOAD

```bash
# LD_PRELOAD carga una biblioteca ANTES que todas las demás.
# Los símbolos de LD_PRELOAD tienen prioridad → pueden reemplazar
# funciones de cualquier otra biblioteca.

# Ejemplo: reemplazar malloc:
# override_malloc.c:
# void *malloc(size_t size) {
#     fprintf(stderr, "malloc(%zu)\n", size);
#     return __libc_malloc(size);  // llamar al original
# }

gcc -shared -fPIC override_malloc.c -o liboverride.so
LD_PRELOAD=./liboverride.so ./program
# Cada malloc del programa imprime su tamaño.

# Usos:
# - Debugging (interceptar funciones)
# - Testing (mock de funciones del sistema)
# - Herramientas: faketime, libeatmydata, tsocks
```

---

## Ejercicios

### Ejercicio 1 — Explorar el enlace dinámico

```bash
# 1. Compilar un programa simple que use printf y sqrt.
# 2. Usar ldd para ver sus dependencias.
# 3. Usar LD_DEBUG=libs para ver cómo se buscan las bibliotecas.
# 4. Usar readelf -d para ver RUNPATH/RPATH.
# 5. Usar objdump -d para encontrar los stubs PLT.
# ¿Cuántas bibliotecas se cargan? ¿De dónde vienen?
```

### Ejercicio 2 — RUNPATH

```bash
# 1. Crear libcalc.so con funciones add, mul.
# 2. Organizar el proyecto:
#    app/
#    ├── bin/program
#    └── lib/libcalc.so
# 3. Compilar program con -Wl,-rpath,'$ORIGIN/../lib'
# 4. Verificar que ./app/bin/program funciona sin LD_LIBRARY_PATH.
# 5. Mover todo el directorio app/ a otra ubicación.
#    ¿Sigue funcionando? ¿Por qué?
```

### Ejercicio 3 — LD_PRELOAD

```bash
# 1. Crear un .so que override time() para siempre retornar 0
#    (epoch: 1970-01-01).
# 2. Crear un programa que imprima la fecha con time() y ctime().
# 3. Ejecutar normal → fecha actual.
# 4. Ejecutar con LD_PRELOAD → Thu Jan  1 00:00:00 1970
# 5. ¿Qué implicaciones de seguridad tiene LD_PRELOAD?
#    ¿Funciona con programas setuid?
```
