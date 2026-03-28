# T03 — Generadores

## Que es un generador

CMake no compila codigo. CMake genera archivos de build para otra
herramienta que si compila. El **generador** (generator) determina
que herramienta se usa: Make, Ninja, Visual Studio, Xcode, etc.

```bash
# El flujo de CMake siempre tiene dos fases:
#
# 1. Configure — CMake lee CMakeLists.txt y genera archivos de build.
# 2. Build     — La herramienta nativa compila el proyecto.
#
# CMakeLists.txt  →  cmake -G "Unix Makefiles"  →  Makefile     →  make
# CMakeLists.txt  →  cmake -G Ninja             →  build.ninja  →  ninja
# CMakeLists.txt  →  cmake -G "Visual Studio"   →  .sln/.vcxproj →  msbuild
#
# El generador se elige en la fase 1 (configure).
# CMake adapta los archivos generados al formato de la herramienta.
```

## Ver generadores disponibles

```bash
# cmake --help muestra los generadores disponibles al final de la salida:
cmake --help
# ...
# Generators
#   Unix Makefiles       = Generates standard UNIX makefiles.
#   Ninja                = Generates build.ninja files.
#   Ninja Multi-Config   = Generates build-<Config>.ninja files.
#   Watcom WMake         = Generates Watcom WMake makefiles.
#   ...

# El generador marcado con un asterisco (*) es el default de la plataforma.
# En Linux, el default es "Unix Makefiles".
```

```bash
# Seleccionar un generador con -G:
cmake -G "Unix Makefiles" -B build    # genera Makefile
cmake -G Ninja -B build               # genera build.ninja

# Las comillas son necesarias si el nombre tiene espacios.
# "Ninja" no las necesita, "Unix Makefiles" si.
```

## Unix Makefiles

El generador por defecto en Linux. Produce un Makefile convencional
que se compila con `make`:

```bash
# Configurar con Unix Makefiles (explicito o implicito):
cmake -G "Unix Makefiles" -B build -S .
# Equivale a (en Linux, donde es el default):
cmake -B build -S .

# El directorio build/ contiene:
ls build/
# CMakeCache.txt  CMakeFiles/  Makefile  cmake_install.cmake
#                              ^^^^^^^^
#                              Este es el archivo que make lee.
```

```bash
# Compilar:
make -C build                  # invocar make directamente
cmake --build build            # forma agnostica (CMake invoca make)

# Ambos producen el mismo resultado.
# cmake --build es la forma recomendada (ver mas abajo).
```

```bash
# Ventajas de Unix Makefiles:
# - Universalmente disponible en cualquier sistema Unix/Linux.
# - No requiere instalar nada adicional.
# - Familiar para quien ya conoce Make.
#
# Desventajas:
# - Mas lento que Ninja en proyectos medianos y grandes.
# - Los Makefiles generados por CMake usan recursive make internamente,
#   lo que limita la eficiencia del paralelismo.
```

## Ninja

Generador alternativo enfocado en velocidad. Produce un archivo
`build.ninja` que se compila con `ninja`:

```bash
# Instalar Ninja (Fedora):
sudo dnf install ninja-build

# Verificar la instalacion:
ninja --version
# 1.12.1
```

```bash
# Configurar con Ninja:
cmake -G Ninja -B build -S .

# El directorio build/ contiene:
ls build/
# CMakeCache.txt  CMakeFiles/  build.ninja  cmake_install.cmake
#                              ^^^^^^^^^^^
#                              Archivo que ninja lee.
```

```bash
# Compilar:
ninja -C build                 # invocar ninja directamente
cmake --build build            # forma agnostica (CMake invoca ninja)
```

```bash
# Por que Ninja es mas rapido que Make:
#
# 1. Non-recursive — Ninja tiene una vista global de todo el build.
#    Los Makefiles generados por CMake usan recursive make (un make
#    por subdirectorio), lo que pierde informacion de dependencias
#    entre subdirectorios y limita el paralelismo.
#
# 2. Paralelismo por defecto — Ninja usa todos los cores automaticamente.
#    Make requiere -j$(nproc) explicito.
#
# 3. Start-up mas rapido — Ninja parsea build.ninja mas rapido que
#    Make parsea los Makefiles generados (que son grandes y complejos).
#
# 4. Minimal stat() — Ninja minimiza las llamadas al sistema para
#    verificar timestamps de archivos.
#
# En proyectos pequenos (< 20 archivos) la diferencia es imperceptible.
# En proyectos grandes (cientos de archivos) Ninja puede ser 2-3x mas rapido.
```

## Comparacion Make vs Ninja

```
| Caracteristica        | Unix Makefiles        | Ninja                   |
|-----------------------|-----------------------|-------------------------|
| Velocidad             | Buena                 | Mejor (2-3x en builds   |
|                       |                       | grandes)                |
| Disponibilidad        | Siempre instalado     | Requiere instalar       |
| Paralelismo default   | Secuencial (-j1)      | Todos los cores         |
| Edicion manual        | Posible (pero no      | No disenado para        |
|                       | recomendado con CMake)| edicion humana          |
| Progress bar          | No                    | Si (muestra [N/M])     |
| Verbose               | VERBOSE=1 make        | ninja -v                |
| Recursive             | Si (con CMake)        | No (global)             |
```

```bash
# Ninja muestra progreso por defecto:
cmake --build build    # con generador Ninja
# [1/6] Building C object CMakeFiles/app.dir/main.c.o
# [2/6] Building C object CMakeFiles/app.dir/utils.c.o
# [3/6] Building C object CMakeFiles/app.dir/parser.c.o
# ...
# [6/6] Linking C executable app

# Make no muestra progreso a menos que se use cmake --build:
cmake --build build    # con generador Makefiles
# [  16%] Building C object CMakeFiles/app.dir/main.c.o
# [  33%] Building C object CMakeFiles/app.dir/utils.c.o
# ...
# [100%] Linking C executable app
# (CMake agrega los porcentajes, no Make nativo)
```

## cmake --build

La forma agnostica de compilar. Funciona con cualquier generador
sin necesidad de saber si el proyecto usa Make, Ninja u otra herramienta:

```bash
# Uso basico:
cmake --build build

# CMake lee CMakeCache.txt en build/ para saber que generador se uso,
# y ejecuta la herramienta correspondiente (make, ninja, msbuild...).
```

```bash
# Flags utiles:
cmake --build build --parallel 8       # compilar con 8 jobs (-j8)
cmake --build build --parallel         # usar todos los cores disponibles
cmake --build build -j 4               # forma corta de --parallel

cmake --build build --target app       # compilar solo el target "app"
cmake --build build --target clean     # limpiar (equivale a make clean)

cmake --build build --clean-first      # limpiar y luego compilar
cmake --build build --verbose          # mostrar los comandos ejecutados
```

```bash
# Ventaja: portabilidad.
# Un script o README que diga "cmake --build build" funciona sin importar
# si el generador es Make, Ninja, o cualquier otro.
# No necesitas saber si ejecutar make, ninja, o msbuild.

# Combinacion comun:
cmake -B build -S .
cmake --build build --parallel
```

## Seleccionar generador

El generador se elige al configurar (primera invocacion de cmake).
Una vez creado el directorio de build, el generador no se puede cambiar:

```bash
# Elegir al configurar:
cmake -G Ninja -B build -S .

# Si build/ ya existe con otro generador, da error:
cmake -G "Unix Makefiles" -B build -S .
# Error: generator mismatch — build/ fue configurado con Ninja.
# Solucion: borrar build/ y reconfigurar.
rm -rf build && cmake -G "Unix Makefiles" -B build -S .
```

```bash
# Establecer un generador default con variable de entorno:
export CMAKE_GENERATOR=Ninja

# Ahora cmake sin -G usa Ninja:
cmake -B build -S .    # usa Ninja (por CMAKE_GENERATOR)

# Se puede poner en ~/.bashrc o ~/.zshrc:
echo 'export CMAKE_GENERATOR=Ninja' >> ~/.zshrc
# Asi todos los proyectos usan Ninja por defecto.
```

## Single-config vs multi-config

Los generadores se clasifican en dos tipos segun como manejan
las configuraciones de build (Debug, Release, etc.):

```bash
# Single-config: un directorio de build por configuracion.
# Makefiles y Ninja son single-config.
cmake -G Ninja -B build-debug   -DCMAKE_BUILD_TYPE=Debug
cmake -G Ninja -B build-release -DCMAKE_BUILD_TYPE=Release
# Cada directorio tiene su propia configuracion.
# Para cambiar, hay que reconfigurar o usar otro directorio.
```

```bash
# Multi-config: un directorio de build para todas las configuraciones.
# Visual Studio, Xcode, y "Ninja Multi-Config" son multi-config.
cmake -G "Ninja Multi-Config" -B build
cmake --build build --config Debug      # compila en Debug
cmake --build build --config Release    # compila en Release
# Ambas configuraciones coexisten en el mismo directorio.
# No se usa CMAKE_BUILD_TYPE con generadores multi-config.
```

```bash
# Ninja Multi-Config existe desde CMake 3.17.
# Es util para CI/CD donde se quieren compilar multiples
# configuraciones sin reconfigurar:
cmake -G "Ninja Multi-Config" -B build
cmake --build build --config Debug
cmake --build build --config Release
# Un solo configure, dos builds.
```

## CMAKE_MAKE_PROGRAM

CMake detecta automaticamente la herramienta de build (make, ninja, etc.)
en el PATH del sistema. Se puede sobreescribir si hay multiples versiones
o si la herramienta esta en una ruta no estandar:

```cmake
# En linea de comandos:
# cmake -DCMAKE_MAKE_PROGRAM=/opt/ninja-1.12/ninja -G Ninja -B build

# Normalmente no es necesario. CMake encuentra make y ninja
# automaticamente si estan en el PATH.
```

## Verbose builds

Para ver los comandos exactos que se ejecutan (flags de gcc, paths, etc.):

```bash
# Forma agnostica (funciona con cualquier generador):
cmake --build build --verbose

# Con Makefiles:
cd build && VERBOSE=1 make
# o:
cd build && make VERBOSE=1

# Con Ninja:
cd build && ninja -v
```

```bash
# Tambien se puede establecer a nivel de CMake:
cmake -B build -DCMAKE_VERBOSE_MAKEFILE=ON
# Los builds posteriores siempre muestran los comandos.
# Util para depurar problemas de compilacion.
```

## Recomendacion practica

```bash
# Estrategia recomendada:
# 1. Usar Ninja si esta disponible (mas rapido, mejor UX).
# 2. Fallback a Makefiles si Ninja no esta instalado.
# 3. Usar cmake --build para portabilidad.

# Configuracion en el shell profile:
export CMAKE_GENERATOR=Ninja

# Flujo de trabajo:
cmake -B build -S .             # usa Ninja (por CMAKE_GENERATOR)
cmake --build build --parallel  # compila con todos los cores
cmake --build build --verbose   # si necesitas ver los comandos
```

---

## Ejercicios

### Ejercicio 1 --- Comparar generadores

```bash
# Crear un proyecto CMake con al menos 5 archivos fuente .c
# (pueden ser triviales: cada uno define una funcion que retorna un int,
# mas un main.c que las llama).
#
# 1. Configurar y compilar con Unix Makefiles:
#    cmake -G "Unix Makefiles" -B build-make -S .
#    time cmake --build build-make --parallel
#
# 2. Configurar y compilar con Ninja:
#    cmake -G Ninja -B build-ninja -S .
#    time cmake --build build-ninja --parallel
#
# 3. Comparar los directorios generados:
#    ls -la build-make/
#    ls -la build-ninja/
#    Identificar el Makefile vs build.ninja.
#
# 4. Ejecutar un verbose build con cada generador:
#    cmake --build build-make --verbose
#    cmake --build build-ninja --verbose
#    Observar que los comandos de gcc son identicos —
#    solo cambia la herramienta que los orquesta.
```

### Ejercicio 2 --- CMAKE_GENERATOR y cmake --build

```bash
# 1. Establecer CMAKE_GENERATOR=Ninja en el shell:
#    export CMAKE_GENERATOR=Ninja
#
# 2. Configurar un proyecto sin -G:
#    cmake -B build -S .
#    Verificar que uso Ninja (buscar build.ninja en build/).
#
# 3. Compilar usando cmake --build (no ninja directamente):
#    cmake --build build --parallel
#
# 4. Compilar un target especifico:
#    cmake --build build --target <nombre_del_ejecutable>
#
# 5. Limpiar y recompilar:
#    cmake --build build --clean-first --parallel
#
# 6. Desactivar CMAKE_GENERATOR y reconfigurar en otro directorio:
#    unset CMAKE_GENERATOR
#    cmake -B build2 -S .
#    Verificar que uso el default de la plataforma (Unix Makefiles).
#    cmake --build build2 funciona igual, sin cambiar nada.
```

### Ejercicio 3 --- Single-config vs multi-config

```bash
# 1. Configurar el mismo proyecto con Ninja (single-config)
#    en dos directorios separados:
#    cmake -G Ninja -B build-debug   -DCMAKE_BUILD_TYPE=Debug
#    cmake -G Ninja -B build-release -DCMAKE_BUILD_TYPE=Release
#
# 2. Compilar ambos y verificar que los flags de compilacion difieren:
#    cmake --build build-debug --verbose    # debe mostrar -g (debug)
#    cmake --build build-release --verbose  # debe mostrar -O3 o -O2
#
# 3. Configurar con Ninja Multi-Config (si CMake >= 3.17):
#    cmake -G "Ninja Multi-Config" -B build-multi
#    cmake --build build-multi --config Debug
#    cmake --build build-multi --config Release
#    Verificar que ambas configuraciones coexisten en el mismo directorio.
#
# 4. Reflexionar: por que el flujo de single-config requiere dos
#    directorios y dos configure, pero multi-config solo uno?
```
