# Bloque 4: Sistemas de Compilación

**Objetivo**: Dominar Make y CMake para gestionar proyectos C/C++ de cualquier tamaño.

## Capítulo 1: Make y Makefiles [A]

### Sección 1: Fundamentos
- **T01 - Reglas**: target, prerequisites, recipe — sintaxis, tabulación obligatoria
- **T02 - Variables**: =, :=, ?=, +=, diferencias de evaluación lazy vs immediate
- **T03 - Reglas implícitas y patrones**: %.o: %.c, variables automáticas ($@, $<, $^, $*)
- **T04 - Phony targets**: .PHONY, clean, all, install

### Sección 2: Make Avanzado
- **T01 - Funciones**: $(wildcard), $(patsubst), $(foreach), $(shell)
- **T02 - Dependencias automáticas**: -MMD, -MP, inclusión de archivos .d
- **T03 - Makefiles recursivos vs no-recursivos**: pros, contras, pattern recomendado
- **T04 - Paralelismo**: make -j, order-only prerequisites

## Capítulo 2: CMake [A]

### Sección 1: Fundamentos
- **T01 - CMakeLists.txt básico**: cmake_minimum_required, project, add_executable
- **T02 - Targets y propiedades**: add_library, target_include_directories, target_link_libraries
- **T03 - Generadores**: Makefiles, Ninja, diferencias, selección
- **T04 - Build types**: Debug, Release, RelWithDebInfo, MinSizeRel

### Sección 2: CMake Intermedio
- **T01 - Variables y caché**: set, option, CMakeCache.txt, -D flags
- **T02 - find_package y módulos**: Config mode vs Module mode, pkg-config integration
- **T03 - Install y export**: destinos de instalación, CMake package config
- **T04 - Subdirectorios y estructura de proyecto**: add_subdirectory, target visibility

## Capítulo 3: pkg-config [A]

### Sección 1: Uso y Configuración
- **T01 - Qué es pkg-config**: .pc files, --cflags, --libs, PKG_CONFIG_PATH
- **T02 - Crear un .pc file**: para bibliotecas propias
- **T03 - Integración con Make y CMake**: patrones idiomáticos
