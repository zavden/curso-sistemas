# T01 — gcov y lcov: Cobertura de Código en C

> **Bloque 17 — Testing e Ingeniería de Software → C01 — Testing en C → S04 — Cobertura en C → T01**

---

## Índice

1. [Qué es la cobertura de código](#1-qué-es-la-cobertura-de-código)
2. [El flujo completo: compilar → ejecutar → medir → visualizar](#2-el-flujo-completo-compilar--ejecutar--medir--visualizar)
3. [Flags de compilación: -fprofile-arcs -ftest-coverage](#3-flags-de-compilación--fprofile-arcs--ftest-coverage)
4. [Archivos generados: .gcno y .gcda](#4-archivos-generados-gcno-y-gcda)
5. [gcov: la herramienta base](#5-gcov-la-herramienta-base)
6. [Anatomía de un archivo .gcov](#6-anatomía-de-un-archivo-gcov)
7. [Opciones de gcov completas](#7-opciones-de-gcov-completas)
8. [Ejemplo completo paso a paso con gcov](#8-ejemplo-completo-paso-a-paso-con-gcov)
9. [lcov: cobertura a nivel de proyecto](#9-lcov-cobertura-a-nivel-de-proyecto)
10. [genhtml: reportes HTML interactivos](#10-genhtml-reportes-html-interactivos)
11. [Flujo lcov completo con ejemplo real](#11-flujo-lcov-completo-con-ejemplo-real)
12. [Filtrado y manipulación de datos con lcov](#12-filtrado-y-manipulación-de-datos-con-lcov)
13. [Cobertura de ramas con gcov y lcov](#13-cobertura-de-ramas-con-gcov-y-lcov)
14. [Integración con Make](#14-integración-con-make)
15. [Integración con CMake](#15-integración-con-cmake)
16. [Cobertura acumulativa: múltiples ejecutables](#16-cobertura-acumulativa-múltiples-ejecutables)
17. [gcovr: alternativa moderna a lcov](#17-gcovr-alternativa-moderna-a-lcov)
18. [Comparación gcov/lcov vs Rust/Go](#18-comparación-gcovlcov-vs-rustgo)
19. [Errores comunes y diagnóstico](#19-errores-comunes-y-diagnóstico)
20. [Ejemplo completo: biblioteca de estructuras de datos](#20-ejemplo-completo-biblioteca-de-estructuras-de-datos)
21. [Programa de práctica](#21-programa-de-práctica)
22. [Ejercicios](#22-ejercicios)

---

## 1. Qué es la cobertura de código

La **cobertura de código** (code coverage) mide qué porcentaje de tu código fuente fue ejecutado durante los tests. No indica que el código sea correcto — indica qué partes fueron **ejercitadas**.

### Tipos de cobertura

```
Tipo                   Qué mide                              Dificultad
─────────────────────────────────────────────────────────────────────────
Line coverage          Líneas ejecutadas / líneas totales     Baja
Function coverage      Funciones llamadas / funciones totales Baja
Branch coverage        Ramas tomadas / ramas posibles         Media
Condition coverage     Sub-expresiones booleanas evaluadas    Alta
MC/DC                  Modified Condition/Decision Coverage   Muy alta
Path coverage          Caminos completos a través del código  Extrema
```

### Relación entre tipos

```
                    Path coverage (más estricta)
                          │
                      MC/DC coverage
                          │
                   Condition coverage
                          │
                    Branch coverage
                          │
                 ┌────────┴────────┐
                 │                 │
           Line coverage    Function coverage
                 │                 │
                 └────────┬────────┘
                          │
                     Sin cobertura
```

**gcov** mide line coverage, function coverage y branch coverage. Para MC/DC y niveles superiores se necesitan herramientas especializadas (BullseyeCoverage, Parasoft C/C++test, etc.).

### Por qué medir cobertura

```
Beneficio                    Detalle
─────────────────────────────────────────────────────────────────────
Detectar código no testeado  Identificar funciones o ramas sin tests
Guiar esfuerzo de testing    Priorizar tests en código descubierto
Prevenir regresiones         Asegurar que código nuevo tenga tests
Requisito de compliance      DO-178C, ISO 26262, IEC 62304 exigen cobertura
Métricas de calidad          Datos objetivos para el equipo
```

### Lo que la cobertura NO dice

```
 ╔══════════════════════════════════════════════════════════════╗
 ║  100% de cobertura ≠ 0 bugs                                ║
 ║                                                             ║
 ║  La cobertura indica qué código SE EJECUTÓ,                ║
 ║  no que el resultado de esa ejecución sea CORRECTO.         ║
 ║                                                             ║
 ║  Un test que ejecuta una función sin verificar su resultado ║
 ║  aumenta la cobertura sin detectar nada.                    ║
 ╚══════════════════════════════════════════════════════════════╝
```

Ejemplo:

```c
// Esta función tiene un bug: debería retornar a - b
int subtract(int a, int b) {
    return a + b;  // BUG
}

// Este "test" da 100% line coverage en subtract()
// pero no detecta el bug
void test_subtract(void) {
    subtract(3, 2);  // Ejecuta la línea, pero no verifica resultado
    // Debería ser: assert(subtract(3, 2) == 1);
}
```

---

## 2. El flujo completo: compilar → ejecutar → medir → visualizar

El proceso de cobertura con gcov/lcov sigue un flujo lineal de cuatro pasos:

```
  ┌─────────────────────────────────────────────────────────────────────────┐
  │                   FLUJO DE COBERTURA gcov/lcov                         │
  │                                                                        │
  │  Paso 1: COMPILAR con instrumentación                                  │
  │  ┌──────────────────────────────────────────────────────────────┐      │
  │  │  gcc -fprofile-arcs -ftest-coverage -o prog prog.c          │      │
  │  │                                                              │      │
  │  │  Genera:  prog      (ejecutable instrumentado)              │      │
  │  │           prog.gcno (notas de grafo — estructura estática)  │      │
  │  └──────────────────────────────────────────────────────────────┘      │
  │                           │                                            │
  │                           ▼                                            │
  │  Paso 2: EJECUTAR los tests                                            │
  │  ┌──────────────────────────────────────────────────────────────┐      │
  │  │  ./prog                                                      │      │
  │  │                                                              │      │
  │  │  Genera:  prog.gcda (datos de arcos — contadores runtime)   │      │
  │  └──────────────────────────────────────────────────────────────┘      │
  │                           │                                            │
  │                           ▼                                            │
  │  Paso 3: MEDIR cobertura                                               │
  │  ┌──────────────────────────────────────────────────────────────┐      │
  │  │  gcov prog.c                                                 │      │
  │  │                                                              │      │
  │  │  Lee:     prog.gcno + prog.gcda                             │      │
  │  │  Genera:  prog.c.gcov (reporte por línea anotado)           │      │
  │  │                                                              │      │
  │  │  --- o con lcov ---                                          │      │
  │  │  lcov --capture --directory . --output-file coverage.info    │      │
  │  │                                                              │      │
  │  │  Lee:     todos los .gcno + .gcda del directorio            │      │
  │  │  Genera:  coverage.info (formato consolidado)               │      │
  │  └──────────────────────────────────────────────────────────────┘      │
  │                           │                                            │
  │                           ▼                                            │
  │  Paso 4: VISUALIZAR                                                    │
  │  ┌──────────────────────────────────────────────────────────────┐      │
  │  │  genhtml coverage.info --output-directory coverage_html      │      │
  │  │                                                              │      │
  │  │  Genera:  coverage_html/index.html (reporte navegable)      │      │
  │  └──────────────────────────────────────────────────────────────┘      │
  │                                                                        │
  └─────────────────────────────────────────────────────────────────────────┘
```

### Componentes del ecosistema

```
Herramienta   Parte de         Función
──────────────────────────────────────────────────────────────────
gcc           GCC              Compila con instrumentación
gcov          GCC              Lee .gcno + .gcda, genera .gcov
lcov          Paquete lcov     Consolida datos gcov de un proyecto
genhtml       Paquete lcov     Genera reportes HTML desde .info
gcovr         Paquete gcovr    Alternativa Python a lcov
```

Instalación:

```bash
# Debian/Ubuntu
sudo apt install gcc lcov

# Fedora/RHEL
sudo dnf install gcc lcov

# Arch
sudo pacman -S gcc lcov

# gcovr (alternativa, requiere Python)
pip install gcovr
```

---

## 3. Flags de compilación: -fprofile-arcs -ftest-coverage

### Los dos flags fundamentales

```
Flag                    Efecto en compilación                    Archivo generado
────────────────────────────────────────────────────────────────────────────────────
-ftest-coverage         Genera notas de estructura del código    .gcno
-fprofile-arcs          Inserta contadores en cada arco del CFG  .gcda (en runtime)
```

**-ftest-coverage** le dice a GCC que genere un archivo `.gcno` (GCC Notes) por cada archivo fuente compilado. Este archivo contiene:

- Número de bloques básicos
- Arcos entre bloques (el grafo de flujo de control — CFG)
- Mapeo de arcos a líneas de código fuente
- Nombres de funciones

**-fprofile-arcs** le dice a GCC que inserte código de instrumentación que cuenta cuántas veces se ejecuta cada arco del CFG. Al terminar el programa, estos contadores se escriben en archivos `.gcda` (GCC Data).

### Atajo: --coverage

El flag `--coverage` es equivalente a `-fprofile-arcs -ftest-coverage` en compilación y a `-lgcov` en enlace:

```bash
# Estas dos líneas son equivalentes:
gcc -fprofile-arcs -ftest-coverage -o prog prog.c -lgcov
gcc --coverage -o prog prog.c
```

### Impacto en la optimización

```
 ╔══════════════════════════════════════════════════════════════╗
 ║  IMPORTANTE: Compilar con -O0 para cobertura precisa        ║
 ║                                                             ║
 ║  Las optimizaciones del compilador (-O1, -O2, -O3) pueden:  ║
 ║  - Eliminar código "muerto" (dead code elimination)         ║
 ║  - Fusionar ramas (branch folding)                          ║
 ║  - Inline funciones (function inlining)                     ║
 ║  - Reordenar bloques básicos                                ║
 ║                                                             ║
 ║  Esto causa reportes imprecisos: líneas que "aparecen"      ║
 ║  ejecutadas cuando no lo fueron, o viceversa.               ║
 ╚══════════════════════════════════════════════════════════════╝
```

Compilación recomendada para cobertura:

```bash
gcc -O0 -g --coverage -o prog prog.c
#    │   │  │
#    │   │  └── instrumentación de cobertura
#    │   └───── información de depuración (útil para correlación)
#    └───────── sin optimización (cobertura precisa)
```

### Qué hace la instrumentación internamente

Cuando compilas con `--coverage`, GCC transforma cada función insertando contadores. Conceptualmente:

```c
// Código original
int max(int a, int b) {
    if (a > b)
        return a;
    else
        return b;
}

// Código instrumentado (conceptual, no literal)
static unsigned long __gcov_counter[4] = {0};

int max(int a, int b) {
    __gcov_counter[0]++;          // Entrada a la función
    if (a > b) {
        __gcov_counter[1]++;      // Rama true
        return a;
    } else {
        __gcov_counter[2]++;      // Rama false
        return b;
    }
    // __gcov_counter[3] para la salida (implícito)
}

// Al terminar el proceso, atexit() escribe __gcov_counter[] → .gcda
```

El grafo de flujo de control (CFG) generado:

```
        ┌─────────────────┐
        │  Bloque 0:      │
        │  entrada         │
        │  __gcov_ctr[0]++ │
        │  if (a > b)     │
        └────────┬────────┘
                 │
          ┌──────┴──────┐
     true │             │ false
          ▼             ▼
   ┌────────────┐ ┌────────────┐
   │  Bloque 1: │ │  Bloque 2: │
   │  ctr[1]++  │ │  ctr[2]++  │
   │  return a  │ │  return b  │
   └────────────┘ └────────────┘
```

### Impacto en rendimiento

La instrumentación añade overhead:

```
Aspecto            Impacto típico
────────────────────────────────────────────
Tamaño binario     +10% a +30%
Velocidad          -15% a -30%
Uso de memoria     +5% a +15%
Escritura .gcda    Unos milisegundos al exit
```

Esto es aceptable para tests pero **nunca** se debe activar en builds de producción.

---

## 4. Archivos generados: .gcno y .gcda

### .gcno (GCC Notes) — Estructura estática

```
Generado en:     Compilación (por -ftest-coverage)
Contenido:       Estructura del código: bloques, arcos, funciones, líneas
Cuándo se crea:  Cada vez que compilas
Uno por cada:    Archivo .c / .o compilado
Nombre:          Mismo que el .o pero con extensión .gcno
                 src/math.c → src/math.gcno (si compilas en el mismo dir)
                 build/math.o → build/math.gcno (si compilas en build/)
```

### .gcda (GCC Data) — Contadores de ejecución

```
Generado en:     Ejecución del programa instrumentado
Contenido:       Contadores: cuántas veces se ejecutó cada arco
Cuándo se crea:  Cuando el proceso termina (exit() o return de main())
Uno por cada:    Archivo .gcno correspondiente
Nombre:          Mismo prefijo que .gcno pero con extensión .gcda
Persistencia:    Se ACUMULA entre ejecuciones (los contadores se suman)
```

### Diagrama de archivos

```
                    COMPILACIÓN                      EJECUCIÓN
                    ──────────                       ─────────

  math.c ──────┐
               │    gcc --coverage
               ├──────────────────►  math.gcno   ─────────┐
               │                     (estructura)          │
               │                                           │  gcov lee
               │                     math.o ──► prog ──►  │  ambos
               │                                   │       │
               │                                   │       │
               │                              ./prog       │
               │                                   │       ▼
               │                              math.gcda ───┘
               │                              (contadores)
               │
  main.c ──────┤
               │    gcc --coverage
               ├──────────────────►  main.gcno ────────────┐
               │                     main.o ──► prog       │
               │                                   │       │  gcov lee
               │                              ./prog       │  ambos
               │                                   │       │
               │                              main.gcda ───┘
               │
```

### Acumulación de .gcda

Cada vez que ejecutas el programa, los contadores en `.gcda` se **suman** a los existentes:

```bash
$ gcc --coverage -o test_math test_math.c math.c

$ ./test_math          # Primera ejecución: crea .gcda con contadores = N
$ ./test_math          # Segunda ejecución: contadores = 2N (acumulados)
$ ./test_math          # Tercera ejecución: contadores = 3N

$ gcov math.c          # Mostrará 3N ejecuciones por línea
```

Para empezar de cero:

```bash
# Eliminar .gcda manualmente
rm -f *.gcda

# O con lcov
lcov --zerocounters --directory .
```

### Ubicación de los archivos

Los `.gcno` y `.gcda` se generan **junto al archivo .o**, no junto al fuente:

```bash
# Compilación en el directorio fuente
$ gcc --coverage -o prog src/math.c src/main.c
# Genera: src/math.gcno, src/main.gcno (junto a los fuentes)
# Tras ejecución: src/math.gcda, src/main.gcda

# Compilación en directorio de build separado
$ gcc --coverage -c src/math.c -o build/math.o
# Genera: build/math.gcno
# Tras ejecución: build/math.gcda

# PERO los .gcno almacenan la ruta absoluta del fuente
# gcov necesita encontrar tanto el .gcno como el .c original
```

### Precaución con recompilación

```
 ╔══════════════════════════════════════════════════════════════╗
 ║  Si recompilas, los .gcno se regeneran.                     ║
 ║  Si los .gcda eran de la compilación anterior,              ║
 ║  gcov dará error: "version mismatch" o datos corruptos.     ║
 ║                                                             ║
 ║  REGLA: Siempre borrar .gcda al recompilar.                 ║
 ╚══════════════════════════════════════════════════════════════╝
```

---

## 5. gcov: la herramienta base

`gcov` es la herramienta incluida con GCC que lee los archivos `.gcno` y `.gcda` y genera reportes de cobertura línea por línea.

### Uso básico

```bash
# Paso 1: Compilar con cobertura
gcc --coverage -O0 -o prog prog.c

# Paso 2: Ejecutar
./prog

# Paso 3: Generar reporte
gcov prog.c
```

La salida de `gcov prog.c`:

```
File 'prog.c'
Lines executed:85.71% of 14
Creating 'prog.c.gcov'
```

### Qué genera gcov

gcov genera un archivo `.gcov` por cada archivo fuente analizado. Es un archivo de texto plano con anotaciones:

```
        -:    0:Source:prog.c
        -:    0:Graph:prog.gcno
        -:    0:Data:prog.gcda
        -:    0:Runs:1
        -:    1:#include <stdio.h>
        -:    2:
        3:    3:int max(int a, int b) {
        3:    4:    if (a > b)
        2:    5:        return a;
        -:    6:    else
        1:    7:        return b;
        -:    8:}
        -:    9:
        1:   10:int main(void) {
        1:   11:    printf("max(3,1)=%d\n", max(3, 1));
        1:   12:    printf("max(1,5)=%d\n", max(1, 5));
        1:   13:    printf("max(7,7)=%d\n", max(7, 7));
        1:   14:    return 0;
        -:   15:}
```

---

## 6. Anatomía de un archivo .gcov

### Formato de cada línea

```
CONTADOR:  LINEA:  CÓDIGO_FUENTE
```

Donde `CONTADOR` puede ser:

```
Marcador     Significado
─────────────────────────────────────────────────────────────
    N        La línea se ejecutó N veces (N > 0)
    -        La línea no es ejecutable (declaraciones, comentarios, llaves)
#####        La línea es ejecutable pero NUNCA se ejecutó
=====        Bloque excepcional no ejecutado (catch, cleanup)
```

### Ejemplo detallado con análisis

```c
// calculator.c
#include <stdio.h>
#include <stdlib.h>

typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV } Operation;

double calculate(double a, double b, Operation op) {
    switch (op) {
        case OP_ADD: return a + b;
        case OP_SUB: return a - b;
        case OP_MUL: return a * b;
        case OP_DIV:
            if (b == 0.0) {
                fprintf(stderr, "Division by zero!\n");
                return 0.0;
            }
            return a / b;
        default:
            fprintf(stderr, "Unknown operation\n");
            return 0.0;
    }
}

int main(void) {
    printf("3 + 2 = %.1f\n", calculate(3, 2, OP_ADD));
    printf("3 - 2 = %.1f\n", calculate(3, 2, OP_SUB));
    printf("3 * 2 = %.1f\n", calculate(3, 2, OP_MUL));
    // No llamamos OP_DIV
    return 0;
}
```

Archivo `.gcov` resultante:

```
        -:    1:// calculator.c
        -:    2:#include <stdio.h>
        -:    3:#include <stdlib.h>
        -:    4:
        -:    5:typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV } Operation;
        -:    6:
        3:    7:double calculate(double a, double b, Operation op) {
        3:    8:    switch (op) {
        1:    9:        case OP_ADD: return a + b;
        1:   10:        case OP_SUB: return a - b;
        1:   11:        case OP_MUL: return a * b;
    #####:   12:        case OP_DIV:
    #####:   13:            if (b == 0.0) {
    #####:   14:                fprintf(stderr, "Division by zero!\n");
    #####:   15:                return 0.0;
        -:   16:            }
    #####:   17:            return a / b;
    #####:   18:        default:
    #####:   19:            fprintf(stderr, "Unknown operation\n");
    #####:   20:            return 0.0;
        -:   21:    }
        -:   22:}
        -:   23:
        1:   24:int main(void) {
        1:   25:    printf("3 + 2 = %.1f\n", calculate(3, 2, OP_ADD));
        1:   26:    printf("3 - 2 = %.1f\n", calculate(3, 2, OP_SUB));
        1:   27:    printf("3 * 2 = %.1f\n", calculate(3, 2, OP_MUL));
        -:   28:    // No llamamos OP_DIV
        1:   29:    return 0;
        -:   30:}
```

### Lectura del reporte

```
Análisis del .gcov:

 ┌──────────────────────────────────────────────────────────────────┐
 │  Lines executed: 10 de 16 ejecutables = 62.5%                   │
 │                                                                  │
 │  Funciones:                                                      │
 │    calculate():  llamada 3 veces                                │
 │    main():       llamada 1 vez                                  │
 │                                                                  │
 │  Código NO cubierto (#####):                                    │
 │    - Líneas 12-17: caso OP_DIV completo (rama y div-by-zero)   │
 │    - Líneas 18-20: caso default (operación desconocida)         │
 │                                                                  │
 │  Acción recomendada:                                             │
 │    - Añadir test con OP_DIV (caso normal y b=0)                 │
 │    - Añadir test con valor inválido para el default             │
 └──────────────────────────────────────────────────────────────────┘
```

### Encabezado del .gcov

Las líneas con número de línea `0` son metadatos:

```
        -:    0:Source:calculator.c      ← Archivo fuente analizado
        -:    0:Graph:calculator.gcno    ← Archivo de notas usado
        -:    0:Data:calculator.gcda     ← Archivo de datos usado
        -:    0:Runs:1                   ← Número de ejecuciones acumuladas
```

---

## 7. Opciones de gcov completas

### Referencia de flags

```
Flag                         Descripción
──────────────────────────────────────────────────────────────────────────
-a, --all-blocks             Mostrar contadores para cada bloque básico
-b, --branch-probabilities   Mostrar probabilidades de ramas
-c, --branch-counts          Mostrar contadores de ramas (no porcentajes)
-d, --display-progress       Mostrar progreso durante procesamiento
-f, --function-summaries     Mostrar resumen por función
-H, --human-readable         Contadores legibles (1.5k en vez de 1500)
-i, --json-format            Salida en formato JSON intermedio (.gcov.json.gz)
-j, --json                   Alias de -i (versiones recientes)
-k, --use-colors             Colorear la salida en terminal
-l, --long-file-names        Nombres largos para archivos de salida
-m, --demangled-names        Desmanglar nombres C++ (no aplica a C puro)
-n, --no-output              No generar archivo .gcov (solo resumen)
-o DIR, --object-directory    Directorio donde buscar .gcno y .gcda
-p, --preserve-paths         Preservar paths completos en nombres de archivo
-r, --relative-only          Solo mostrar archivos con path relativo
-s DIR, --source-prefix       Eliminar prefijo DIR de las rutas
-t, --stdout                 Enviar salida a stdout en vez de a archivo
-u, --unconditional-branches Mostrar ramas incondicionales
-v, --version                Mostrar versión
-w, --verbose                Salida verbose
-x, --hash-filenames         Usar hash MD5 en nombres de archivo
```

### Ejemplos de uso

#### Resumen por función (-f)

```bash
$ gcov -f calculator.c
Function 'calculate'
Lines executed:55.56% of 9
Branches executed:40.00% of 10

Function 'main'
Lines executed:100.00% of 5
No branches

File 'calculator.c'
Lines executed:62.50% of 16
Branches executed:40.00% of 10
Creating 'calculator.c.gcov'
```

#### Cobertura de ramas (-b)

```bash
$ gcov -b calculator.c
File 'calculator.c'
Lines executed:62.50% of 16
Branches executed:40.00% of 10
Taken at least once:30.00% of 10
Calls executed:60.00% of 5
Creating 'calculator.c.gcov'
```

El `.gcov` con `-b` incluye información de ramas:

```
        3:    8:    switch (op) {
branch  0 taken 33%
branch  1 taken 33%
branch  2 taken 33%
branch  3 never executed
branch  4 never executed
        1:    9:        case OP_ADD: return a + b;
        1:   10:        case OP_SUB: return a - b;
        1:   11:        case OP_MUL: return a * b;
    #####:   12:        case OP_DIV:
    #####:   13:            if (b == 0.0) {
branch  0 never executed
branch  1 never executed
```

#### Salida JSON (-i)

```bash
$ gcov -i calculator.c
# Genera calculator.c.gcov.json.gz

$ zcat calculator.c.gcov.json.gz | python3 -m json.tool | head -30
{
    "gcc_version": "13.2.0",
    "current_working_directory": "/home/user/project",
    "data_file": "calculator.gcda",
    "format_version": "2",
    "files": [
        {
            "file": "calculator.c",
            "functions": [
                {
                    "name": "calculate",
                    "demangled_name": "calculate",
                    "start_line": 7,
                    "start_column": 8,
                    "end_line": 22,
                    "end_column": 1,
                    "blocks": 12,
                    "blocks_executed": 6,
                    "execution_count": 3
                }
            ],
            "lines": [...]
        }
    ]
}
```

#### Solo resumen, sin generar archivo (-n)

```bash
$ gcov -n calculator.c
File 'calculator.c'
Lines executed:62.50% of 16
```

#### Directorio de objetos (-o)

```bash
# Cuando compilas en un directorio de build separado
$ gcc --coverage -c src/math.c -o build/math.o
$ gcc --coverage -o build/test_math build/math.o build/test_math.o

$ ./build/test_math

# gcov necesita saber dónde están los .gcno y .gcda
$ gcov -o build src/math.c
```

---

## 8. Ejemplo completo paso a paso con gcov

### Código fuente

```c
// string_utils.h
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>
#include <stdbool.h>

// Retorna la longitud de la cadena (sin contar '\0')
size_t str_length(const char *s);

// Copia src a dst, retorna dst. dst debe tener espacio suficiente.
char *str_copy(char *dst, const char *src);

// Compara dos cadenas. Retorna 0 si iguales, <0 si a<b, >0 si a>b.
int str_compare(const char *a, const char *b);

// Busca el carácter c en s. Retorna puntero a la primera ocurrencia o NULL.
char *str_find_char(const char *s, char c);

// Convierte la cadena a mayúsculas in-place. Retorna s.
char *str_to_upper(char *s);

// Verifica si la cadena es un palíndromo (ignora case).
bool str_is_palindrome(const char *s);

#endif // STRING_UTILS_H
```

```c
// string_utils.c
#include "string_utils.h"
#include <ctype.h>

size_t str_length(const char *s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

char *str_copy(char *dst, const char *src) {
    if (!dst || !src) return dst;
    char *original = dst;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
    return original;
}

int str_compare(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *str_find_char(const char *s, char c) {
    if (!s) return NULL;
    while (*s) {
        if (*s == c) return (char *)s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return NULL;
}

char *str_to_upper(char *s) {
    if (!s) return NULL;
    char *p = s;
    while (*p) {
        *p = toupper((unsigned char)*p);
        p++;
    }
    return s;
}

bool str_is_palindrome(const char *s) {
    if (!s) return false;
    size_t len = str_length(s);
    if (len <= 1) return true;
    size_t left = 0;
    size_t right = len - 1;
    while (left < right) {
        if (tolower((unsigned char)s[left]) != tolower((unsigned char)s[right])) {
            return false;
        }
        left++;
        right--;
    }
    return true;
}
```

```c
// test_string_utils.c — Tests parciales (intencionalmente incompletos)
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "string_utils.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN(name) do { \
    printf("  %-40s", #name); \
    name(); \
    tests_passed++; \
    printf(" PASS\n"); \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf(" FAIL (line %d)\n", __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf(" FAIL (line %d)\n", __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

// === Tests para str_length ===
TEST(test_length_normal) {
    ASSERT_EQ(str_length("hello"), 5);
    ASSERT_EQ(str_length(""), 0);
    ASSERT_EQ(str_length("a"), 1);
}

TEST(test_length_null) {
    ASSERT_EQ(str_length(NULL), 0);
}

// === Tests para str_copy ===
TEST(test_copy_normal) {
    char buf[32];
    str_copy(buf, "hello");
    ASSERT_STR_EQ(buf, "hello");
}

// No testeamos str_copy con NULL

// === Tests para str_compare ===
TEST(test_compare_equal) {
    ASSERT_EQ(str_compare("abc", "abc"), 0);
}

TEST(test_compare_less) {
    assert(str_compare("abc", "abd") < 0);
}

// No testeamos str_compare con NULL

// === No testeamos str_find_char ===

// === Tests para str_to_upper ===
TEST(test_to_upper_basic) {
    char buf[] = "hello";
    str_to_upper(buf);
    ASSERT_STR_EQ(buf, "HELLO");
}

// No testeamos str_to_upper con NULL
// No testeamos str_to_upper con caracteres no alfabéticos

// === No testeamos str_is_palindrome ===

int main(void) {
    printf("=== String Utils Tests ===\n");

    RUN(test_length_normal);
    RUN(test_length_null);
    RUN(test_copy_normal);
    RUN(test_compare_equal);
    RUN(test_compare_less);
    RUN(test_to_upper_basic);

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
```

### Paso a paso en terminal

```bash
# 1. Compilar con cobertura
$ gcc -O0 -g --coverage -o test_string_utils \
    test_string_utils.c string_utils.c

$ ls *.gcno
string_utils.gcno  test_string_utils.gcno

# 2. Ejecutar los tests
$ ./test_string_utils
=== String Utils Tests ===
  test_length_normal                       PASS
  test_length_null                         PASS
  test_copy_normal                         PASS
  test_compare_equal                       PASS
  test_compare_less                        PASS
  test_to_upper_basic                      PASS

6 passed, 0 failed

$ ls *.gcda
string_utils.gcda  test_string_utils.gcda

# 3. Generar reporte gcov
$ gcov -f string_utils.c
Function 'str_length'
Lines executed:100.00% of 5

Function 'str_copy'
Lines executed:83.33% of 6

Function 'str_compare'
Lines executed:77.78% of 9

Function 'str_find_char'
Lines executed:0.00% of 6

Function 'str_to_upper'
Lines executed:83.33% of 6

Function 'str_is_palindrome'
Lines executed:0.00% of 10

File 'string_utils.c'
Lines executed:52.38% of 42
Creating 'string_utils.c.gcov'
```

### Análisis del .gcov generado

```
        -:    1:// string_utils.c
        -:    2:#include "string_utils.h"
        -:    3:#include <ctype.h>
        -:    4:
        5:    5:size_t str_length(const char *s) {
        5:    6:    if (!s) return 0;            ← 5 llamadas (3 en test + 2 internas)
        4:    7:    size_t len = 0;              ← 4 veces (1 vez fue NULL)
       14:    8:    while (s[len] != '\0') {
       10:    9:        len++;
        -:   10:    }
        4:   11:    return len;
        -:   12:}
        -:   13:
        1:   14:char *str_copy(char *dst, const char *src) {
        1:   15:    if (!dst || !src) return dst;  ← rama NULL nunca tomada
        1:   16:    char *original = dst;
        6:   17:    while (*src) {
        5:   18:        *dst++ = *src++;
        -:   19:    }
        1:   20:    *dst = '\0';
        1:   21:    return original;
        -:   22:}
        -:   23:
        2:   24:int str_compare(const char *a, const char *b) {
        2:   25:    if (!a && !b) return 0;
    #####:   26:    if (!a) return -1;            ← NUNCA ejecutado
    #####:   27:    if (!b) return 1;             ← NUNCA ejecutado
        8:   28:    while (*a && *b && *a == *b) {
        6:   29:        a++;
        6:   30:        b++;
        -:   31:    }
        2:   32:    return (unsigned char)*a - (unsigned char)*b;
        -:   33:}
        -:   34:
    #####:   35:char *str_find_char(const char *s, char c) {
    #####:   36:    if (!s) return NULL;
    #####:   37:    while (*s) {
    #####:   38:        if (*s == c) return (char *)s;
    #####:   39:        s++;
        -:   40:    }
    #####:   41:    if (c == '\0') return (char *)s;
    #####:   42:    return NULL;
        -:   43:}
        -:   44:
        1:   45:char *str_to_upper(char *s) {
        1:   46:    if (!s) return NULL;
        1:   47:    char *p = s;
        6:   48:    while (*p) {
        5:   49:        *p = toupper((unsigned char)*p);
        5:   50:        p++;
        -:   51:    }
        1:   52:    return s;
        -:   53:}
        -:   54:
    #####:   55:bool str_is_palindrome(const char *s) {
    #####:   56:    if (!s) return false;
    #####:   57:    size_t len = str_length(s);
    #####:   58:    if (len <= 1) return true;
    #####:   59:    size_t left = 0;
    #####:   60:    size_t right = len - 1;
    #####:   61:    while (left < right) {
    #####:   62:        if (tolower((unsigned char)s[left]) != tolower((unsigned char)s[right])) {
    #####:   63:            return false;
        -:   64:        }
    #####:   65:        left++;
    #####:   66:        right--;
        -:   67:    }
    #####:   68:    return true;
        -:   69:}
```

### Mapa visual de cobertura

```
  string_utils.c — Cobertura: 52.38%

  str_length      ████████████████████  100%   ← Completamente cubierta
  str_copy        ████████████████░░░░   83%   ← Falta caso NULL
  str_compare     ███████████████░░░░░   78%   ← Faltan casos NULL
  str_find_char   ░░░░░░░░░░░░░░░░░░░░    0%  ← SIN TESTS
  str_to_upper    ████████████████░░░░   83%   ← Falta caso NULL
  str_is_palindr  ░░░░░░░░░░░░░░░░░░░░    0%  ← SIN TESTS

  ████ = cubierto    ░░░░ = no cubierto
```

---

## 9. lcov: cobertura a nivel de proyecto

`gcov` procesa un archivo a la vez. **lcov** consolida los datos de cobertura de todo un proyecto en un solo archivo `.info` y permite operaciones sobre esos datos.

### Comandos principales de lcov

```
Comando                                    Función
──────────────────────────────────────────────────────────────────────────
lcov --capture -d DIR -o FILE.info         Capturar datos de cobertura
lcov --zerocounters -d DIR                 Borrar todos los .gcda
lcov --initial -d DIR -o base.info         Capturar línea base (0 ejecuciones)
lcov --add-tracefile A -a B -o merged      Combinar dos archivos .info
lcov --remove FILE.info PATTERN -o OUT     Eliminar archivos del reporte
lcov --extract FILE.info PATTERN -o OUT    Extraer solo archivos que coincidan
lcov --list FILE.info                      Listar contenido del archivo .info
lcov --summary FILE.info                   Resumen estadístico
lcov --diff FILE.info DIFF -o OUT          Aplicar diff al reporte
```

### Opciones importantes

```
Opción                          Descripción
──────────────────────────────────────────────────────────────────────────
-d, --directory DIR             Directorio donde buscar .gcda/.gcno
-o, --output-file FILE          Archivo de salida
-b, --base-directory DIR        Directorio base para paths relativos
-t, --test-name NAME            Nombre del test (para reportes)
--rc lcov_branch_coverage=1     Activar cobertura de ramas
--no-external                   Excluir archivos del sistema (/usr/...)
-q, --quiet                     Reducir la salida
--ignore-errors ERRTYPE         Ignorar errores (source, gcov, graph...)
--gcov-tool TOOL                Usar gcov alternativo (ej: llvm-cov gcov)
--list-full-path                Mostrar rutas completas en --list
```

### El archivo .info

El archivo `.info` que genera lcov tiene un formato de texto plano específico:

```
TN:test_name
SF:/home/user/project/src/string_utils.c
FN:5,str_length
FN:14,str_copy
FN:24,str_compare
FN:35,str_find_char
FN:45,str_to_upper
FN:55,str_is_palindrome
FNDA:5,str_length
FNDA:1,str_copy
FNDA:2,str_compare
FNDA:0,str_find_char
FNDA:1,str_to_upper
FNDA:0,str_is_palindrome
FNF:6
FNH:4
DA:5,5
DA:6,5
DA:7,4
DA:8,14
DA:9,10
DA:11,4
...
DA:68,0
LF:42
LH:22
BRDA:6,0,0,1
BRDA:6,0,1,4
...
BRF:24
BRH:14
end_of_record
```

Decodificación de campos:

```
Campo    Significado
──────────────────────────────────────────────────────────
TN       Test Name
SF       Source File (ruta al archivo fuente)
FN       Function Name: FN:línea_inicio,nombre
FNDA     Function Data: FNDA:conteo_ejecuciones,nombre
FNF      Functions Found (total de funciones)
FNH      Functions Hit (funciones ejecutadas al menos una vez)
DA       Data (línea): DA:número_línea,conteo_ejecuciones
LF       Lines Found (total de líneas ejecutables)
LH       Lines Hit (líneas ejecutadas al menos una vez)
BRDA     Branch Data: BRDA:línea,bloque,rama,conteo
BRF      Branches Found
BRH      Branches Hit
```

---

## 10. genhtml: reportes HTML interactivos

`genhtml` toma un archivo `.info` y genera un directorio con reportes HTML navegables.

### Uso básico

```bash
genhtml coverage.info --output-directory coverage_html
```

### Opciones de genhtml

```
Opción                              Descripción
──────────────────────────────────────────────────────────────────────────
-o, --output-directory DIR          Directorio de salida
-t, --title TITLE                   Título del reporte
-p, --prefix PREFIX                 Eliminar prefijo de rutas
-s, --sort                          Ordenar archivos por % cobertura
--legend                            Mostrar leyenda de colores
--highlight                         Resaltar código con syntax highlighting
--no-function-coverage              No mostrar cobertura de funciones
--branch-coverage                   Mostrar cobertura de ramas
--demangle-cpp                      Desmanglar nombres C++
--rc genhtml_hi_limit=90            Umbral "alto" (verde) - default 90%
--rc genhtml_med_limit=75           Umbral "medio" (amarillo) - default 75%
--dark-mode                         Tema oscuro (lcov >= 2.0)
--flat                              Sin jerarquía de directorios
--frames                            Usar frames HTML (deprecated)
--css-file FILE                     CSS personalizado
--num-spaces N                      Espacios por tab (default 8)
--function-coverage                 Incluir cobertura de funciones
```

### Estructura del reporte HTML

```
coverage_html/
├── index.html                    ← Página principal: resumen del proyecto
├── index-sort-f.html             ← Ordenado por nombre de archivo
├── index-sort-l.html             ← Ordenado por cobertura de líneas
├── gcov.css                      ← Estilos CSS
├── glass.png                     ← Iconos
├── updown.png
├── src/
│   ├── index.html                ← Resumen del directorio src/
│   ├── string_utils.c.gcov.html  ← Cobertura línea por línea
│   └── math_utils.c.gcov.html
└── amber.png
```

### Visualización del reporte

La página principal muestra:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    LCOV - code coverage report                      │
│                                                                     │
│  Test: Unit Tests                          Date: 2026-04-05         │
│                                                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Lines:      ████████████░░░░░░░░  62.5%  (22/42)            │  │
│  │  Functions:  ████████████████░░░░  66.7%  (4/6)              │  │
│  │  Branches:   ██████████░░░░░░░░░░  58.3%  (14/24)           │  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                     │
│  Directory             Line Cov    Functions    Branches             │
│  ─────────────────────────────────────────────────────────────────  │
│  src/                  62.5%       66.7%        58.3%               │
│    string_utils.c      52.4%       4/6          14/24               │
│    test_string_utils.c 100.0%      7/7          -                   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

La vista por archivo muestra cada línea con colores:

```
  Color        Significado
  ─────────────────────────────────────
  Verde        Línea ejecutada
  Rojo         Línea NO ejecutada (#####)
  Blanco       Línea no ejecutable (-, comentarios)
  Naranja      Rama parcialmente cubierta
```

---

## 11. Flujo lcov completo con ejemplo real

### Proyecto de ejemplo

Usaremos el proyecto `string_utils` de la sección 8, añadiendo el flujo lcov completo.

### Paso 1: Compilar con cobertura

```bash
$ gcc -O0 -g --coverage -o test_string_utils \
    test_string_utils.c string_utils.c
```

### Paso 2: Capturar línea base (opcional pero recomendado)

La línea base captura todas las líneas ejecutables con contadores a 0. Esto es importante porque sin ella, archivos que nunca se ejecutan no aparecerían en el reporte.

```bash
$ lcov --capture --initial \
       --directory . \
       --output-file coverage_base.info

Capturing coverage data from .
Found gcov version: 13.2.0
Using intermediate gcov format
Scanning . for .gcno files ...
Found 2 graph files in .
Processing string_utils.gcno
Processing test_string_utils.gcno
Writing data to coverage_base.info
Summary coverage rate:
  lines......: 0.0% (0 of 56)
  functions..: 0.0% (0 of 8)
```

### Paso 3: Ejecutar los tests

```bash
$ ./test_string_utils
=== String Utils Tests ===
  test_length_normal                       PASS
  test_length_null                         PASS
  test_copy_normal                         PASS
  test_compare_equal                       PASS
  test_compare_less                        PASS
  test_to_upper_basic                      PASS

6 passed, 0 failed
```

### Paso 4: Capturar cobertura real

```bash
$ lcov --capture \
       --directory . \
       --output-file coverage_test.info

Capturing coverage data from .
Found gcov version: 13.2.0
Scanning . for .gcda files ...
Found 2 data files in .
Processing string_utils.gcda
Processing test_string_utils.gcda
Writing data to coverage_test.info
Summary coverage rate:
  lines......: 85.7% (48 of 56)
  functions..: 87.5% (7 of 8)
```

### Paso 5: Combinar base + test

```bash
$ lcov --add-tracefile coverage_base.info \
       --add-tracefile coverage_test.info \
       --output-file coverage_total.info

Combining tracefiles.
Writing data to coverage_total.info
Summary coverage rate:
  lines......: 85.7% (48 of 56)
  functions..: 87.5% (7 of 8)
```

¿Por qué combinar? La base asegura que archivos/funciones con 0 ejecuciones aparezcan en el reporte. Sin ella, si un archivo entero no tiene tests, simplemente no aparece — dando una falsa sensación de buena cobertura.

### Paso 6: Filtrar archivos no relevantes

```bash
# Eliminar archivos del sistema y archivos de test del reporte
$ lcov --remove coverage_total.info \
       '/usr/*' \
       '*/test_*' \
       --output-file coverage_filtered.info

Reading tracefile coverage_total.info
Removing /usr/include/ctype.h
Removing test_string_utils.c
Writing data to coverage_filtered.info
Summary coverage rate:
  lines......: 52.4% (22 of 42)
  functions..: 66.7% (4 of 6)
```

Ahora el reporte solo muestra la cobertura del código de producción.

### Paso 7: Generar HTML

```bash
$ genhtml coverage_filtered.info \
         --output-directory coverage_html \
         --title "String Utils - Unit Tests" \
         --legend \
         --branch-coverage

Reading data file coverage_filtered.info
Found 1 entries.
Found common filename prefix "/home/user/project"
Writing .css and .png files.
Generating output.
Processing file src/string_utils.c
Writing directory view page.
Overall coverage rate:
  lines......: 52.4% (22 of 42)
  functions..: 66.7% (4 of 6)
  branches...: 58.3% (14 of 24)
```

### Paso 8: Ver el reporte

```bash
# Abrir en navegador
$ xdg-open coverage_html/index.html       # Linux
$ open coverage_html/index.html            # macOS

# O servir con Python para acceso remoto
$ python3 -m http.server 8080 -d coverage_html
# Abrir http://localhost:8080
```

### Paso 9: Verificar con --list y --summary

```bash
$ lcov --list coverage_filtered.info
Reading tracefile coverage_filtered.info
                      |Lines      |Functions |Branches
Filename              |Rate    Num|Rate   Num|Rate   Num
==========================================================
[/home/user/project/]
string_utils.c        |52.4%   42|66.7%    6|58.3%   24
==========================================================
              Total:  |52.4%   42|66.7%    6|58.3%   24

$ lcov --summary coverage_filtered.info
Reading tracefile coverage_filtered.info
Summary coverage rate:
  lines......: 52.4% (22 of 42)
  functions..: 66.7% (4 of 6)
  branches...: 58.3% (14 of 24)
```

### Script completo

```bash
#!/bin/bash
# coverage.sh — Flujo completo de cobertura
set -euo pipefail

PROJECT="String Utils"
SRC="string_utils.c"
TEST="test_string_utils.c"
BINARY="test_string_utils"
OUT_DIR="coverage_html"

echo "=== Limpieza ==="
rm -f *.gcno *.gcda *.gcov *.info
rm -rf "$OUT_DIR"

echo "=== Compilación con cobertura ==="
gcc -O0 -g --coverage -o "$BINARY" "$TEST" "$SRC"

echo "=== Línea base ==="
lcov --capture --initial -d . -o base.info --quiet

echo "=== Ejecución de tests ==="
./"$BINARY"

echo "=== Captura de cobertura ==="
lcov --capture -d . -o test.info --quiet

echo "=== Combinación base + test ==="
lcov -a base.info -a test.info -o total.info --quiet

echo "=== Filtrado ==="
lcov --remove total.info '/usr/*' '*/test_*' -o filtered.info --quiet

echo "=== Resumen ==="
lcov --summary filtered.info

echo "=== Generación HTML ==="
genhtml filtered.info \
    -o "$OUT_DIR" \
    --title "$PROJECT" \
    --legend \
    --branch-coverage \
    --quiet

echo ""
echo "Reporte generado en: $OUT_DIR/index.html"
```

---

## 12. Filtrado y manipulación de datos con lcov

### --remove: eliminar por patrón

```bash
# Eliminar archivos del sistema
lcov --remove coverage.info '/usr/*' -o filtered.info

# Eliminar archivos de test
lcov --remove coverage.info '*/test_*' '*/tests/*' -o filtered.info

# Eliminar archivos de terceros
lcov --remove coverage.info '*/vendor/*' '*/third_party/*' -o filtered.info

# Combinar exclusiones
lcov --remove coverage.info \
    '/usr/*' \
    '*/test_*' \
    '*/tests/*' \
    '*/vendor/*' \
    '*/mock_*' \
    -o filtered.info
```

### --extract: incluir solo por patrón

```bash
# Solo archivos del directorio src/
lcov --extract coverage.info '*/src/*' -o src_only.info

# Solo un archivo específico
lcov --extract coverage.info '*string_utils*' -o string_only.info

# Solo archivos .c (excluyendo .h)
lcov --extract coverage.info '*.c' -o c_only.info
```

### --add-tracefile: combinar datos

```bash
# Combinar cobertura de múltiples ejecuciones
lcov -a unit_tests.info \
     -a integration_tests.info \
     -a fuzz_tests.info \
     -o combined.info
```

### Comparar dos runs

```bash
# Run 1: tests unitarios
./test_unit
lcov --capture -d . -o unit.info

# Limpiar contadores (no recompile)
lcov --zerocounters -d .

# Run 2: tests de integración
./test_integration
lcov --capture -d . -o integration.info

# Comparar
lcov --list unit.info
lcov --list integration.info

# Combinar para cobertura total
lcov -a unit.info -a integration.info -o combined.info
lcov --list combined.info
```

### Restar cobertura (qué cubre B que no cubre A)

lcov no tiene resta directa, pero puedes comparar los reportes:

```bash
# Generar HTML separados
genhtml unit.info -o cov_unit --title "Unit Tests"
genhtml integration.info -o cov_integration --title "Integration Tests"
genhtml combined.info -o cov_combined --title "Combined"
```

---

## 13. Cobertura de ramas con gcov y lcov

### Activar cobertura de ramas

Por defecto, ni gcov ni lcov reportan ramas. Para activarlas:

```bash
# gcov: flag -b
gcov -b string_utils.c

# lcov: rc option
lcov --capture -d . -o coverage.info --rc lcov_branch_coverage=1

# genhtml: flag --branch-coverage
genhtml coverage.info -o html --branch-coverage

# O configurar globalmente en ~/.lcovrc
echo 'lcov_branch_coverage = 1' >> ~/.lcovrc
echo 'genhtml_branch_coverage = 1' >> ~/.lcovrc
```

### Archivo de configuración ~/.lcovrc

```bash
# ~/.lcovrc — Configuración global de lcov

# Activar cobertura de ramas por defecto
lcov_branch_coverage = 1
genhtml_branch_coverage = 1

# Umbrales de color para genhtml
genhtml_hi_limit = 90
genhtml_med_limit = 75

# Número de espacios por tab
genhtml_num_spaces = 4

# Ordenar por cobertura (menor primero)
genhtml_sort = 1

# Mostrar leyenda
genhtml_legend = 1

# Encoding
genhtml_charset = UTF-8
```

### Interpretación de ramas en gcov

```bash
$ gcov -b string_utils.c
```

El `.gcov` con ramas:

```
        5:    5:size_t str_length(const char *s) {
        5:    6:    if (!s) return 0;
branch  0 taken 20% (fallthrough)       ← 1 de 5 veces s fue NULL
branch  1 taken 80%                      ← 4 de 5 veces s no fue NULL
        4:    7:    size_t len = 0;
       14:    8:    while (s[len] != '\0') {
branch  0 taken 71%                      ← 10 de 14 iteraciones continuaron
branch  1 taken 29%                      ← 4 de 14 salieron del while
       10:    9:        len++;
        -:   10:    }
        4:   11:    return len;
        -:   12:}
```

### Diagrama de ramas

```
  str_length("hello")

  if (!s)  ──── branch 0: false (s != NULL) ──► continuar
           └─── branch 1: true  (s == NULL) ──► return 0

  while (s[len] != '\0')
           ──── branch 0: true  (seguir iterando) ──► len++
           └─── branch 1: false (fin de cadena)   ──► return len

  Para 100% branch coverage en str_length, necesitas:
    - Al menos un test con s = NULL  (branch 1 de if)
    - Al menos un test con s != NULL (branch 0 de if)
    - Al menos un test con cadena no vacía (branch 0 de while)
    - Al menos un test con cadena (branch 1 de while se toma siempre al final)
```

### Branch coverage vs line coverage

```c
int classify_age(int age) {
    if (age < 0) {
        return -1;       // Error
    } else if (age < 18) {
        return 0;        // Menor
    } else if (age < 65) {
        return 1;        // Adulto
    } else {
        return 2;        // Senior
    }
}
```

```
Test con solo classify_age(30):
  Line coverage:   55.6%  (5 de 9 líneas ejecutables)
  Branch coverage: 25.0%  (2 de 8 ramas: solo age>=0 y age>=18 y age<65)

Tests completos: classify_age(-1), classify_age(10), classify_age(30), classify_age(70):
  Line coverage:   100%   (9 de 9)
  Branch coverage: 100%   (8 de 8)
```

### Ramas en expresiones complejas

```c
// Cada && y || genera ramas adicionales
if (a > 0 && b > 0 && c > 0) {
    // ...
}
```

gcc genera ramas por cada sub-expresión (short-circuit evaluation):

```
branch 0: a > 0   → true/false
branch 1: b > 0   → true/false (solo si a > 0)
branch 2: c > 0   → true/false (solo si a > 0 && b > 0)
```

Para 100% branch coverage se necesitan tests que cubran:

```
Test 1: a=1, b=1, c=1   → todas true
Test 2: a=0, b=?, c=?   → primera false (short-circuit)
Test 3: a=1, b=0, c=?   → segunda false
Test 4: a=1, b=1, c=0   → tercera false
```

---

## 14. Integración con Make

### Makefile con targets de cobertura

```makefile
# Makefile

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
LDFLAGS  =

# Archivos fuente
SRC      = string_utils.c
TEST_SRC = test_string_utils.c
HDR      = string_utils.h

# Binario de test
TEST_BIN = test_string_utils

# Directorio de reporte HTML
COV_DIR  = coverage_html

# === Build normal (sin cobertura) ===
.PHONY: all
all: CFLAGS += -O2
all: $(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TEST_SRC) $(SRC)

# === Build con cobertura ===
.PHONY: coverage
coverage: CFLAGS  += -O0 -g --coverage
coverage: LDFLAGS += --coverage
coverage: clean $(TEST_BIN) run-tests gcov-report lcov-report
	@echo ""
	@echo "Reporte HTML: $(COV_DIR)/index.html"

# === Ejecutar tests ===
.PHONY: run-tests
run-tests: $(TEST_BIN)
	./$(TEST_BIN)

# === Reporte gcov (texto) ===
.PHONY: gcov-report
gcov-report:
	gcov -f -b $(SRC)
	@echo ""
	@echo "Archivos .gcov generados"

# === Reporte lcov (HTML) ===
.PHONY: lcov-report
lcov-report:
	lcov --capture --initial -d . -o base.info --quiet
	lcov --capture -d . -o test.info --quiet
	lcov -a base.info -a test.info -o total.info --quiet
	lcov --remove total.info '/usr/*' '*/test_*' -o filtered.info --quiet
	genhtml filtered.info \
		-o $(COV_DIR) \
		--title "$(notdir $(CURDIR))" \
		--legend \
		--branch-coverage \
		--quiet
	@echo ""
	lcov --summary filtered.info

# === Solo resumen (sin HTML) ===
.PHONY: coverage-summary
coverage-summary: CFLAGS  += -O0 -g --coverage
coverage-summary: LDFLAGS += --coverage
coverage-summary: clean $(TEST_BIN) run-tests
	lcov --capture -d . -o coverage.info --quiet
	lcov --remove coverage.info '/usr/*' '*/test_*' -o filtered.info --quiet
	lcov --summary filtered.info

# === Limpieza ===
.PHONY: clean
clean:
	rm -f $(TEST_BIN) *.o *.gcno *.gcda *.gcov *.info
	rm -rf $(COV_DIR)

# === Verificar umbral mínimo ===
COV_THRESHOLD ?= 80

.PHONY: coverage-check
coverage-check: coverage-summary
	@LINES=$$(lcov --summary filtered.info 2>&1 | \
		grep 'lines' | \
		grep -oP '[\d.]+(?=%)'); \
	echo "Cobertura de líneas: $${LINES}%"; \
	if [ $$(echo "$${LINES} < $(COV_THRESHOLD)" | bc -l) -eq 1 ]; then \
		echo "ERROR: Cobertura $${LINES}% < umbral $(COV_THRESHOLD)%"; \
		exit 1; \
	else \
		echo "OK: Cobertura $${LINES}% >= umbral $(COV_THRESHOLD)%"; \
	fi
```

### Uso del Makefile

```bash
# Build y tests normales
$ make
$ make run-tests

# Cobertura completa con reporte HTML
$ make coverage

# Solo resumen numérico
$ make coverage-summary

# Verificar umbral (falla si < 80%)
$ make coverage-check

# Umbral personalizado
$ make coverage-check COV_THRESHOLD=60

# Limpiar
$ make clean
```

### Makefile para proyecto multi-directorio

```makefile
# Makefile para proyecto con src/ y tests/

CC        = gcc
CFLAGS    = -Wall -Wextra -std=c11 -Isrc
COV_FLAGS = -O0 -g --coverage

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build

# Encontrar todos los fuentes
SRCS      = $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
OBJS      = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

TEST_BIN  = $(BUILD_DIR)/run_tests
COV_DIR   = coverage_html

# Build de cobertura
.PHONY: coverage
coverage: CFLAGS += $(COV_FLAGS)
coverage: clean $(TEST_BIN)
	./$(TEST_BIN)
	lcov --capture --initial -d $(BUILD_DIR) -o $(BUILD_DIR)/base.info -q
	lcov --capture -d $(BUILD_DIR) -o $(BUILD_DIR)/test.info -q
	lcov -a $(BUILD_DIR)/base.info -a $(BUILD_DIR)/test.info \
		-o $(BUILD_DIR)/total.info -q
	lcov --remove $(BUILD_DIR)/total.info \
		'/usr/*' '*/tests/*' \
		-o $(BUILD_DIR)/filtered.info -q
	genhtml $(BUILD_DIR)/filtered.info \
		-o $(COV_DIR) \
		--title "Project Coverage" \
		--legend --branch-coverage -q
	lcov --summary $(BUILD_DIR)/filtered.info

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(COV_DIR)
```

---

## 15. Integración con CMake

### CMakeLists.txt con opción de cobertura

```cmake
cmake_minimum_required(VERSION 3.16)
project(string_utils C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Opción de cobertura (OFF por defecto)
option(ENABLE_COVERAGE "Enable code coverage with gcov" OFF)

# Biblioteca principal
add_library(string_utils src/string_utils.c)
target_include_directories(string_utils PUBLIC src)

# Ejecutable de test
add_executable(test_string_utils tests/test_string_utils.c)
target_link_libraries(test_string_utils PRIVATE string_utils)

# Configurar cobertura si está habilitada
if(ENABLE_COVERAGE)
    # Verificar que usamos GCC
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "Coverage requires GCC (found ${CMAKE_C_COMPILER_ID})")
    endif()

    # Flags de cobertura
    target_compile_options(string_utils PRIVATE -O0 -g --coverage)
    target_link_options(string_utils PRIVATE --coverage)

    target_compile_options(test_string_utils PRIVATE -O0 -g --coverage)
    target_link_options(test_string_utils PRIVATE --coverage)

    message(STATUS "Coverage enabled: compile with -O0 --coverage")
endif()

# CTest
enable_testing()
add_test(NAME string_utils_tests COMMAND test_string_utils)
```

### Uso

```bash
# Build normal
mkdir build && cd build
cmake ..
make
ctest

# Build con cobertura
mkdir build-cov && cd build-cov
cmake -DENABLE_COVERAGE=ON ..
make
ctest

# Generar reporte
lcov --capture -d . -o coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' -o filtered.info
genhtml filtered.info -o coverage_html --branch-coverage
```

### Módulo CMake reutilizable para cobertura

```cmake
# cmake/CodeCoverage.cmake
#
# Uso:
#   include(cmake/CodeCoverage.cmake)
#   add_coverage_target(
#       NAME coverage
#       EXECUTABLE test_string_utils
#       DEPENDENCIES string_utils test_string_utils
#       EXCLUDE '/usr/*' '*/tests/*'
#   )

find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

if(NOT LCOV_PATH)
    message(FATAL_ERROR "lcov not found! Install with: sudo apt install lcov")
endif()

if(NOT GENHTML_PATH)
    message(FATAL_ERROR "genhtml not found! Install with: sudo apt install lcov")
endif()

function(add_coverage_target)
    set(options "")
    set(oneValueArgs NAME EXECUTABLE)
    set(multiValueArgs DEPENDENCIES EXCLUDE)
    cmake_parse_arguments(COV
        "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Target personalizado
    add_custom_target(${COV_NAME}

        # Limpiar contadores previos
        COMMAND ${LCOV_PATH} --zerocounters -d ${CMAKE_BINARY_DIR} -q

        # Capturar línea base
        COMMAND ${LCOV_PATH} --capture --initial
            -d ${CMAKE_BINARY_DIR}
            -o ${CMAKE_BINARY_DIR}/base.info -q

        # Ejecutar tests
        COMMAND ${COV_EXECUTABLE}

        # Capturar datos
        COMMAND ${LCOV_PATH} --capture
            -d ${CMAKE_BINARY_DIR}
            -o ${CMAKE_BINARY_DIR}/test.info -q

        # Combinar
        COMMAND ${LCOV_PATH}
            -a ${CMAKE_BINARY_DIR}/base.info
            -a ${CMAKE_BINARY_DIR}/test.info
            -o ${CMAKE_BINARY_DIR}/total.info -q

        # Filtrar
        COMMAND ${LCOV_PATH} --remove ${CMAKE_BINARY_DIR}/total.info
            ${COV_EXCLUDE}
            -o ${CMAKE_BINARY_DIR}/filtered.info -q

        # Generar HTML
        COMMAND ${GENHTML_PATH} ${CMAKE_BINARY_DIR}/filtered.info
            -o ${CMAKE_BINARY_DIR}/coverage_html
            --title "${PROJECT_NAME}"
            --legend --branch-coverage -q

        # Mostrar resumen
        COMMAND ${LCOV_PATH} --summary ${CMAKE_BINARY_DIR}/filtered.info

        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${COV_DEPENDENCIES}
        COMMENT "Generating coverage report..."
        VERBATIM
    )

    # Mensaje informativo
    message(STATUS "Coverage target '${COV_NAME}' created")
    message(STATUS "  Run: cmake --build . --target ${COV_NAME}")
    message(STATUS "  Report: ${CMAKE_BINARY_DIR}/coverage_html/index.html")
endfunction()
```

### CMakeLists.txt usando el módulo

```cmake
cmake_minimum_required(VERSION 3.16)
project(string_utils C)

set(CMAKE_C_STANDARD 11)

# Biblioteca y tests
add_library(string_utils src/string_utils.c)
target_include_directories(string_utils PUBLIC src)

add_executable(test_string_utils tests/test_string_utils.c)
target_link_libraries(test_string_utils PRIVATE string_utils)

enable_testing()
add_test(NAME unit_tests COMMAND test_string_utils)

# Cobertura (solo si se pide)
option(ENABLE_COVERAGE "Enable coverage" OFF)

if(ENABLE_COVERAGE)
    # Aplicar flags de cobertura
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

    include(cmake/CodeCoverage.cmake)

    add_coverage_target(
        NAME coverage
        EXECUTABLE test_string_utils
        DEPENDENCIES string_utils test_string_utils
        EXCLUDE '/usr/*' '*/tests/*' '*/test_*'
    )
endif()
```

```bash
# Uso
$ cmake -B build-cov -DENABLE_COVERAGE=ON
$ cmake --build build-cov
$ cmake --build build-cov --target coverage
# Abre build-cov/coverage_html/index.html
```

---

## 16. Cobertura acumulativa: múltiples ejecutables

En proyectos reales, suele haber múltiples ejecutables de test. Los datos `.gcda` se acumulan automáticamente.

### Escenario

```
proyecto/
├── src/
│   ├── math_utils.c
│   ├── string_utils.c
│   └── file_utils.c
└── tests/
    ├── test_math.c          ← Tests de math_utils
    ├── test_strings.c       ← Tests de string_utils
    ├── test_files.c         ← Tests de file_utils
    └── test_integration.c   ← Tests que usan varios módulos
```

### Estrategia 1: Ejecutar todo y capturar una vez

```bash
# Compilar todos los tests con cobertura
gcc -O0 -g --coverage -Isrc -o test_math    tests/test_math.c    src/math_utils.c
gcc -O0 -g --coverage -Isrc -o test_strings tests/test_strings.c src/string_utils.c
gcc -O0 -g --coverage -Isrc -o test_files   tests/test_files.c   src/file_utils.c
gcc -O0 -g --coverage -Isrc -o test_integ   tests/test_integration.c \
    src/math_utils.c src/string_utils.c src/file_utils.c

# Limpiar contadores
lcov --zerocounters -d .

# Ejecutar todos los tests — los .gcda se acumulan
./test_math
./test_strings
./test_files
./test_integ

# Capturar cobertura total (ya acumulada)
lcov --capture -d . -o coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' -o filtered.info
genhtml filtered.info -o coverage_html --branch-coverage
```

### Estrategia 2: Capturar por separado y combinar

```bash
# Test 1: math
lcov --zerocounters -d .
./test_math
lcov --capture -d . -o math.info -q

# Test 2: strings
lcov --zerocounters -d .
./test_strings
lcov --capture -d . -o strings.info -q

# Test 3: files
lcov --zerocounters -d .
./test_files
lcov --capture -d . -o files.info -q

# Test 4: integration
lcov --zerocounters -d .
./test_integ
lcov --capture -d . -o integ.info -q

# Combinar todo
lcov -a math.info \
     -a strings.info \
     -a files.info \
     -a integ.info \
     -o combined.info

# Filtrar y generar HTML
lcov --remove combined.info '/usr/*' '*/tests/*' -o filtered.info
genhtml filtered.info -o coverage_html --branch-coverage
```

Ventaja de la estrategia 2: puedes ver qué tests cubren qué código individualmente, y puedes generar reportes separados.

### Estrategia 2 con CMake y CTest

```cmake
# CMakeLists.txt
enable_testing()

add_test(NAME math_tests    COMMAND test_math)
add_test(NAME string_tests  COMMAND test_strings)
add_test(NAME file_tests    COMMAND test_files)
add_test(NAME integ_tests   COMMAND test_integration)
```

```bash
# CTest ejecuta todos los tests → .gcda se acumulan
cmake --build build-cov
cd build-cov
ctest --output-on-failure
lcov --capture -d . -o coverage.info
```

### Problema: compilación compartida vs separada

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  CUIDADO: Si compilas el mismo .c en múltiples ejecutables     ║
 ║  con rutas de .o diferentes, cada uno genera su propio .gcda.  ║
 ║                                                                 ║
 ║  gcc -o test_math tests/test_math.c src/math_utils.c           ║
 ║  gcc -o test_integ tests/test_integ.c src/math_utils.c         ║
 ║                                                                 ║
 ║  Ambos escriben a src/math_utils.gcda → ¡se sobreescriben!     ║
 ║                                                                 ║
 ║  SOLUCIÓN: Usar un directorio de build separado por test,      ║
 ║  o compilar como biblioteca compartida (.a o .so).             ║
 ╚══════════════════════════════════════════════════════════════════╝
```

Solución con biblioteca estática:

```bash
# Compilar fuentes una sola vez como biblioteca
gcc -O0 -g --coverage -c src/math_utils.c -o build/math_utils.o
gcc -O0 -g --coverage -c src/string_utils.c -o build/string_utils.o
ar rcs build/libproject.a build/math_utils.o build/string_utils.o

# Enlazar cada test con la misma biblioteca
gcc -O0 -g --coverage -o build/test_math tests/test_math.c \
    -Lbuild -lproject -Isrc
gcc -O0 -g --coverage -o build/test_integ tests/test_integ.c \
    -Lbuild -lproject -Isrc

# Ahora los .gcda de math_utils y string_utils están en build/
# y se acumulan correctamente entre ejecuciones
./build/test_math
./build/test_integ

lcov --capture -d build -o coverage.info
```

---

## 17. gcovr: alternativa moderna a lcov

`gcovr` es una herramienta escrita en Python que es una alternativa a lcov. Ofrece las mismas funcionalidades pero con algunas ventajas.

### Ventajas de gcovr sobre lcov

```
Característica              lcov               gcovr
───────────────────────────────────────────────────────────────
Lenguaje                    Perl               Python
Instalación                 apt/dnf            pip install
Salida JSON                 No nativa          Sí (--json)
Salida Cobertura XML        No nativa          Sí (--cobertura-xml)
Salida Sonarqube            No                 Sí (--sonarqube)
Salida CSV                  No                 Sí (--csv)
Salida texto en terminal    No (solo --list)   Sí (default)
Filtrado por regex          Patrones glob      Regex completo
Un solo comando             No (lcov+genhtml)  Sí
Soporte Clang/LLVM          Parcial            Nativo
```

### Uso básico

```bash
# Resumen en terminal (default)
$ gcovr
------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines    Exec  Cover   Missing
------------------------------------------------------------------------------
src/string_utils.c                            42      22    52%   26-27,35-42,55-68
------------------------------------------------------------------------------
TOTAL                                         42      22    52%
------------------------------------------------------------------------------

# HTML
$ gcovr --html-details -o coverage.html

# Solo el directorio src/
$ gcovr --root . --filter src/

# Excluir tests
$ gcovr --exclude 'tests/.*'

# Con ramas
$ gcovr --branches

# JSON para herramientas externas
$ gcovr --json -o coverage.json

# Cobertura XML (para CI: GitLab, Jenkins, etc.)
$ gcovr --cobertura-xml -o coverage.xml

# Sonarqube
$ gcovr --sonarqube -o sonar-coverage.xml

# Verificar umbral (falla si no se cumple)
$ gcovr --fail-under-line 80 --fail-under-branch 60
```

### gcovr con CMake

```cmake
# cmake/CodeCoverageGcovr.cmake
find_program(GCOVR_PATH gcovr)

if(NOT GCOVR_PATH)
    message(FATAL_ERROR "gcovr not found! Install with: pip install gcovr")
endif()

function(add_gcovr_target)
    set(oneValueArgs NAME EXECUTABLE)
    set(multiValueArgs DEPENDENCIES EXCLUDE)
    cmake_parse_arguments(COV "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Construir opciones de exclusión
    set(EXCLUDE_OPTS "")
    foreach(PATTERN ${COV_EXCLUDE})
        list(APPEND EXCLUDE_OPTS --exclude "${PATTERN}")
    endforeach()

    # Target para reporte HTML
    add_custom_target(${COV_NAME}
        COMMAND ${COV_EXECUTABLE}
        COMMAND ${GCOVR_PATH}
            --root ${CMAKE_SOURCE_DIR}
            --object-directory ${CMAKE_BINARY_DIR}
            ${EXCLUDE_OPTS}
            --branches
            --html-details
            -o ${CMAKE_BINARY_DIR}/coverage.html
            --print-summary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${COV_DEPENDENCIES}
        COMMENT "Generating coverage report with gcovr..."
    )

    # Target para solo resumen en terminal
    add_custom_target(${COV_NAME}-summary
        COMMAND ${COV_EXECUTABLE}
        COMMAND ${GCOVR_PATH}
            --root ${CMAKE_SOURCE_DIR}
            --object-directory ${CMAKE_BINARY_DIR}
            ${EXCLUDE_OPTS}
            --branches
            --print-summary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${COV_DEPENDENCIES}
    )

    # Target para verificar umbral
    add_custom_target(${COV_NAME}-check
        COMMAND ${COV_EXECUTABLE}
        COMMAND ${GCOVR_PATH}
            --root ${CMAKE_SOURCE_DIR}
            --object-directory ${CMAKE_BINARY_DIR}
            ${EXCLUDE_OPTS}
            --fail-under-line 80
            --fail-under-branch 60
            --print-summary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS ${COV_DEPENDENCIES}
    )
endfunction()
```

### Comparación de comandos lcov vs gcovr

```
Tarea                         lcov + genhtml                              gcovr
─────────────────────────────────────────────────────────────────────────────────────
Reporte terminal              lcov --list cov.info                        gcovr
Reporte HTML                  lcov --capture -d . -o c.info &&            gcovr --html-details -o cov.html
                              genhtml c.info -o html
Excluir archivos              lcov --remove c.info '/usr/*'               gcovr --exclude '/usr/.*'
Ramas                         lcov --rc lcov_branch_coverage=1            gcovr --branches
Verificar umbral              Script bash manual                          gcovr --fail-under-line 80
XML Cobertura                 No nativo                                   gcovr --cobertura-xml
JSON                          No nativo                                   gcovr --json
Combinar ejecuciones          lcov -a a.info -a b.info -o c.info          gcovr --add-tracefile a.json
                                                                                   --add-tracefile b.json
```

---

## 18. Comparación gcov/lcov vs Rust/Go

### Tabla comparativa

```
Aspecto              C (gcov/lcov)                   Rust (cargo-tarpaulin/     Go (go test -cover)
                                                      llvm-cov)
──────────────────────────────────────────────────────────────────────────────────────────────────────
Herramienta          gcov (incluida con GCC)          cargo-tarpaulin            go tool cover
                                                      cargo-llvm-cov             (integrado)

Compilación          Manual: --coverage               cargo tarpaulin            go test -coverprofile
                     Flags explícitos                  (automático)               (automático)

Granularidad         Línea + rama + función            Línea + rama               Línea (statement)
                                                      (llvm-cov: MC/DC)

Instrumentación      GCC inserta contadores            LLVM source-based          Compilador Go inserta
                     en arcos del CFG                  instrumentation            contadores por bloque

Formato datos        .gcno + .gcda (binario)           profdata (LLVM)            coverprofile (texto)

Formato reporte      .gcov (texto), .info (lcov)       HTML, lcov, JSON           HTML, func, texto
                     HTML (genhtml)

Ramas                gcov -b / lcov --rc               llvm-cov: sí               No nativo
                     branch_coverage=1                 tarpaulin: parcial         (go test -covermode=
                                                                                   atomic cuenta ramas)

Combinar runs        lcov -a a.info -a b.info          tarpaulin --all            go test -coverprofile
                                                                                   (todos los packages)

Filtrar              lcov --remove/--extract            #[cfg(not(tarpaulin))]     -coverpkg=pkg/...
                     gcovr --exclude                    excludes en config

Umbral               Script manual o                   tarpaulin --fail-under     No integrado
                     gcovr --fail-under-line            cargo-llvm-cov (externo)   (script manual)

CI                   lcov XML + script                 cargo-tarpaulin            go test -coverprofile
                     gcovr --cobertura-xml              --ciserver=github          + goveralls/codecov
                     → Jenkins/GitLab/GitHub            --out Xml

Overhead             Moderado (-15% a -30%)            Bajo (-5% a -15%)          Muy bajo (-3% a -5%)

Setup                Complejo (flags + lcov +           Un comando                 Un flag
                     genhtml + filtros)
```

### Ejemplo en cada lenguaje

**C (gcov/lcov):**

```bash
gcc -O0 -g --coverage -o tests test_math.c math.c
./tests
lcov --capture -d . -o cov.info
lcov --remove cov.info '/usr/*' -o filtered.info
genhtml filtered.info -o html --branch-coverage
# 5 comandos, 4 archivos intermedios
```

**Rust (cargo-tarpaulin):**

```bash
cargo tarpaulin --out Html
# 1 comando, reporte directo
```

**Go (go test):**

```bash
go test -coverprofile=cover.out ./...
go tool cover -html=cover.out -o cover.html
# 2 comandos
```

### Lo que C puede aprender

```
Lección de Rust/Go             Aplicación en C
───────────────────────────────────────────────────────────────────
Un solo comando                Crear script/Makefile target: make coverage
Integrado en el build          CMake module con add_coverage_target()
Formato estándar para CI       Usar gcovr --cobertura-xml
Umbral automático              gcovr --fail-under-line 80
Sin archivos intermedios       gcovr combina captura + reporte
```

---

## 19. Errores comunes y diagnóstico

### Error 1: "version mismatch"

```
$ gcov math.c
math.gcno:version '14.0', prefer version '13.2'
```

**Causa**: El .gcno fue generado con una versión de GCC diferente a la de gcov.

**Solución**:

```bash
# Verificar versiones
gcc --version
gcov --version

# Deben ser la misma versión mayor
# Si tienes múltiples versiones, especificar:
gcov-14 math.c
# O con lcov:
lcov --gcov-tool gcov-14 --capture -d . -o coverage.info
```

### Error 2: "cannot open notes file"

```
$ gcov math.c
math.gcno:cannot open notes file
```

**Causa**: gcov no encuentra el archivo .gcno.

**Solución**:

```bash
# Verificar que existe
ls *.gcno

# Si compilaste en otro directorio, especificar -o
gcov -o build/ src/math.c

# Verificar que compilaste con --coverage
gcc --coverage -c math.c -o math.o  # Debe generar math.gcno
```

### Error 3: "no .gcda files found"

```
$ lcov --capture -d . -o coverage.info
...
geninfo: WARNING: no .gcda files found in . - skipping!
```

**Causa**: No ejecutaste el programa, o los .gcda se escribieron en otro directorio.

**Solución**:

```bash
# 1. Ejecutar el programa
./test_program

# 2. Verificar que se crearon
find . -name "*.gcda"

# 3. Si están en otro directorio, apuntar lcov allí
lcov --capture -d build/ -o coverage.info

# 4. El programa debe terminar limpiamente (exit o return de main)
#    Si termina con kill -9, los .gcda NO se escriben
```

### Error 4: Datos inconsistentes tras recompilación

```
$ gcov math.c
math.gcda:stamp mismatch with notes file
```

**Causa**: Recompilaste sin borrar los .gcda anteriores.

**Solución**:

```bash
# Siempre limpiar antes de recompilar
rm -f *.gcda
# O
lcov --zerocounters -d .
# Luego recompilar y re-ejecutar
```

### Error 5: Cobertura incluye archivos del sistema

```
$ lcov --list coverage.info
/usr/include/ctype.h                      100%
/usr/include/string.h                     100%
src/math.c                                 52%
```

**Causa**: lcov captura todo por defecto, incluyendo headers del sistema.

**Solución**:

```bash
# Filtrar
lcov --remove coverage.info '/usr/*' -o filtered.info

# O usar --no-external
lcov --capture -d . -o coverage.info --no-external
```

### Error 6: Líneas marcadas como no ejecutables cuando deberían serlo

```
        -:   10:    x = 5;    ← Debería ser ejecutable
```

**Causa**: Compilaste con optimización (-O1 o superior). El compilador eliminó o fusionó la línea.

**Solución**:

```bash
# SIEMPRE usar -O0 para cobertura
gcc -O0 -g --coverage -o prog prog.c
```

### Error 7: El programa crashea y no genera .gcda

```bash
$ ./test_program
Segmentation fault (core dumped)
$ ls *.gcda
ls: cannot access '*.gcda': No such file or directory
```

**Causa**: Los .gcda se escriben al terminar con `atexit()`. Un crash (SIGSEGV, SIGABRT) no ejecuta `atexit()`.

**Solución**:

```bash
# Opción 1: Llamar __gcov_flush() antes del crash (en el handler)
# Solo para debugging, no para producción

# Opción 2: Usar __gcov_dump() en señales (GCC >= 11)
```

```c
#include <signal.h>

// GCC internal — no es parte de la API pública
extern void __gcov_dump(void);
extern void __gcov_reset(void);

void crash_handler(int sig) {
    __gcov_dump();  // Forzar escritura de .gcda
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(void) {
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    // ... tests ...
}
```

### Error 8: fork() duplica contadores

```
 ╔══════════════════════════════════════════════════════════════╗
 ║  Si tu test hace fork(), los contadores se duplican:        ║
 ║  tanto el padre como el hijo escriben al mismo .gcda.       ║
 ║  Los contadores se suman → valores inflados.                ║
 ║                                                             ║
 ║  Solución: Llamar __gcov_reset() en el proceso hijo         ║
 ║  inmediatamente después del fork().                         ║
 ╚══════════════════════════════════════════════════════════════╝
```

```c
pid_t pid = fork();
if (pid == 0) {
    // Proceso hijo
    extern void __gcov_reset(void);
    __gcov_reset();  // Evitar contadores duplicados
    // ... código del hijo ...
    exit(0);
}
```

### Tabla resumen de errores

```
Síntoma                          Causa probable                    Solución
──────────────────────────────────────────────────────────────────────────────────
version 'X', prefer 'Y'         gcc y gcov distintas versiones    Igualar versiones
cannot open notes file           .gcno no encontrado               -o DIR o recompilar
no .gcda files found             Programa no ejecutado             Ejecutar ./prog
stamp mismatch                   Recompilación sin limpiar         rm *.gcda y re-ejecutar
Archivos /usr/* en reporte       No se filtraron                   --remove '/usr/*'
Líneas - cuando deberían ser N   Compilación con -O1+              Usar -O0
Crash sin .gcda                  atexit() no se ejecuta            __gcov_dump() en handler
Contadores inflados              fork() duplica                    __gcov_reset() en hijo
.gcov muestra 0% en .h           Headers no se compilan            Normal — son declaraciones
```

---

## 20. Ejemplo completo: biblioteca de estructuras de datos

### Proyecto: cola circular (ring buffer)

```
ring_buffer/
├── src/
│   ├── ring_buffer.h
│   └── ring_buffer.c
├── tests/
│   ├── test_ring_buffer.c
│   └── test_edge_cases.c
├── Makefile
└── cmake/
    └── CodeCoverage.cmake
```

### ring_buffer.h

```c
// src/ring_buffer.h
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    size_t   capacity;
    size_t   head;       // Índice de escritura
    size_t   tail;       // Índice de lectura
    size_t   count;      // Elementos actuales
    bool     overwrite;  // Si true, sobreescribe datos viejos al llenar
} RingBuffer;

// Crear un ring buffer con capacidad dada.
// overwrite: si true, push() en buffer lleno sobreescribe el dato más antiguo.
// Retorna NULL si falla la asignación.
RingBuffer *ring_buffer_create(size_t capacity, bool overwrite);

// Liberar todos los recursos del ring buffer.
void ring_buffer_destroy(RingBuffer *rb);

// Insertar un byte. Retorna true si se insertó, false si lleno (y !overwrite).
bool ring_buffer_push(RingBuffer *rb, uint8_t data);

// Extraer un byte. Retorna true si había datos, false si vacío.
bool ring_buffer_pop(RingBuffer *rb, uint8_t *data);

// Ver el siguiente byte sin extraerlo. Retorna true si hay datos.
bool ring_buffer_peek(const RingBuffer *rb, uint8_t *data);

// Insertar N bytes de un array. Retorna cantidad efectivamente insertada.
size_t ring_buffer_write(RingBuffer *rb, const uint8_t *data, size_t len);

// Extraer N bytes a un array. Retorna cantidad efectivamente leída.
size_t ring_buffer_read(RingBuffer *rb, uint8_t *data, size_t len);

// Número de elementos en el buffer.
size_t ring_buffer_count(const RingBuffer *rb);

// Espacio libre disponible.
size_t ring_buffer_free_space(const RingBuffer *rb);

// ¿Está lleno?
bool ring_buffer_is_full(const RingBuffer *rb);

// ¿Está vacío?
bool ring_buffer_is_empty(const RingBuffer *rb);

// Vaciar el buffer (reset).
void ring_buffer_clear(RingBuffer *rb);

// Imprimir estado del buffer (debug).
void ring_buffer_dump(const RingBuffer *rb);

#endif // RING_BUFFER_H
```

### ring_buffer.c

```c
// src/ring_buffer.c
#include "ring_buffer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

RingBuffer *ring_buffer_create(size_t capacity, bool overwrite) {
    if (capacity == 0) return NULL;

    RingBuffer *rb = malloc(sizeof(RingBuffer));
    if (!rb) return NULL;

    rb->buffer = malloc(capacity);
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }

    rb->capacity  = capacity;
    rb->head      = 0;
    rb->tail      = 0;
    rb->count     = 0;
    rb->overwrite = overwrite;

    return rb;
}

void ring_buffer_destroy(RingBuffer *rb) {
    if (!rb) return;
    free(rb->buffer);
    free(rb);
}

bool ring_buffer_push(RingBuffer *rb, uint8_t data) {
    if (!rb) return false;

    if (rb->count == rb->capacity) {
        if (!rb->overwrite) return false;

        // Sobreescribir: mover tail (descartamos el más antiguo)
        rb->buffer[rb->head] = data;
        rb->head = (rb->head + 1) % rb->capacity;
        rb->tail = (rb->tail + 1) % rb->capacity;
        return true;
    }

    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;
    return true;
}

bool ring_buffer_pop(RingBuffer *rb, uint8_t *data) {
    if (!rb || rb->count == 0) return false;

    if (data) {
        *data = rb->buffer[rb->tail];
    }
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;
    return true;
}

bool ring_buffer_peek(const RingBuffer *rb, uint8_t *data) {
    if (!rb || rb->count == 0) return false;

    if (data) {
        *data = rb->buffer[rb->tail];
    }
    return true;
}

size_t ring_buffer_write(RingBuffer *rb, const uint8_t *data, size_t len) {
    if (!rb || !data) return 0;

    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_push(rb, data[i])) break;
        written++;
    }
    return written;
}

size_t ring_buffer_read(RingBuffer *rb, uint8_t *data, size_t len) {
    if (!rb || !data) return 0;

    size_t read_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_pop(rb, &data[i])) break;
        read_count++;
    }
    return read_count;
}

size_t ring_buffer_count(const RingBuffer *rb) {
    if (!rb) return 0;
    return rb->count;
}

size_t ring_buffer_free_space(const RingBuffer *rb) {
    if (!rb) return 0;
    return rb->capacity - rb->count;
}

bool ring_buffer_is_full(const RingBuffer *rb) {
    if (!rb) return false;
    return rb->count == rb->capacity;
}

bool ring_buffer_is_empty(const RingBuffer *rb) {
    if (!rb) return true;
    return rb->count == 0;
}

void ring_buffer_clear(RingBuffer *rb) {
    if (!rb) return;
    rb->head  = 0;
    rb->tail  = 0;
    rb->count = 0;
}

void ring_buffer_dump(const RingBuffer *rb) {
    if (!rb) {
        printf("RingBuffer: NULL\n");
        return;
    }

    printf("RingBuffer[cap=%zu, count=%zu, head=%zu, tail=%zu, overwrite=%s]\n",
           rb->capacity, rb->count, rb->head, rb->tail,
           rb->overwrite ? "true" : "false");

    printf("  Data: [");
    for (size_t i = 0; i < rb->capacity; i++) {
        if (i > 0) printf(", ");
        // Marcar posiciones especiales
        if (i == rb->tail && i == rb->head && rb->count > 0) {
            printf("*H/T:%02x", rb->buffer[i]);
        } else if (i == rb->head) {
            printf("*H:%02x", rb->buffer[i]);
        } else if (i == rb->tail && rb->count > 0) {
            printf("*T:%02x", rb->buffer[i]);
        } else {
            printf("%02x", rb->buffer[i]);
        }
    }
    printf("]\n");
}
```

### test_ring_buffer.c — Tests intencionalmente parciales

```c
// tests/test_ring_buffer.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/ring_buffer.h"

static int passed = 0;

#define TEST(name) static void name(void)
#define RUN(name) do { printf("  %-45s", #name); name(); passed++; printf("PASS\n"); } while(0)

// === Creación ===
TEST(test_create_normal) {
    RingBuffer *rb = ring_buffer_create(16, false);
    assert(rb != NULL);
    assert(ring_buffer_count(rb) == 0);
    assert(ring_buffer_free_space(rb) == 16);
    assert(ring_buffer_is_empty(rb));
    assert(!ring_buffer_is_full(rb));
    ring_buffer_destroy(rb);
}

TEST(test_create_zero_capacity) {
    RingBuffer *rb = ring_buffer_create(0, false);
    assert(rb == NULL);
}

// === Push / Pop ===
TEST(test_push_pop_single) {
    RingBuffer *rb = ring_buffer_create(8, false);
    assert(ring_buffer_push(rb, 0x42));
    assert(ring_buffer_count(rb) == 1);

    uint8_t data;
    assert(ring_buffer_pop(rb, &data));
    assert(data == 0x42);
    assert(ring_buffer_count(rb) == 0);
    ring_buffer_destroy(rb);
}

TEST(test_push_pop_multiple) {
    RingBuffer *rb = ring_buffer_create(4, false);
    for (uint8_t i = 0; i < 4; i++) {
        assert(ring_buffer_push(rb, i + 10));
    }
    assert(ring_buffer_is_full(rb));
    assert(!ring_buffer_push(rb, 99));  // Buffer lleno, no overwrite

    uint8_t data;
    for (uint8_t i = 0; i < 4; i++) {
        assert(ring_buffer_pop(rb, &data));
        assert(data == i + 10);
    }
    assert(ring_buffer_is_empty(rb));
    ring_buffer_destroy(rb);
}

// === Peek ===
TEST(test_peek) {
    RingBuffer *rb = ring_buffer_create(4, false);
    ring_buffer_push(rb, 0xAA);

    uint8_t data;
    assert(ring_buffer_peek(rb, &data));
    assert(data == 0xAA);
    assert(ring_buffer_count(rb) == 1);  // peek no consume
    ring_buffer_destroy(rb);
}

// === Clear ===
TEST(test_clear) {
    RingBuffer *rb = ring_buffer_create(8, false);
    ring_buffer_push(rb, 1);
    ring_buffer_push(rb, 2);
    ring_buffer_push(rb, 3);

    ring_buffer_clear(rb);
    assert(ring_buffer_count(rb) == 0);
    assert(ring_buffer_is_empty(rb));
    ring_buffer_destroy(rb);
}

// NOTA: No testeamos:
//   - ring_buffer_write / ring_buffer_read (bulk)
//   - Modo overwrite (overwrite=true)
//   - Wraparound circular (head vuelve al inicio)
//   - NULL como argumento en varias funciones
//   - ring_buffer_dump
//   - pop con data=NULL
//   - peek en buffer vacío

int main(void) {
    printf("=== Ring Buffer Tests ===\n");
    RUN(test_create_normal);
    RUN(test_create_zero_capacity);
    RUN(test_push_pop_single);
    RUN(test_push_pop_multiple);
    RUN(test_peek);
    RUN(test_clear);
    printf("\n%d tests passed\n", passed);
    return 0;
}
```

### Makefile

```makefile
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -Isrc
SRC      = src/ring_buffer.c
TESTS    = tests/test_ring_buffer.c
TEST_BIN = test_ring_buffer
COV_DIR  = coverage_html

.PHONY: all test coverage clean

all: $(TEST_BIN)

$(TEST_BIN): $(TESTS) $(SRC)
	$(CC) $(CFLAGS) -O2 -o $@ $^

test: $(TEST_BIN)
	./$(TEST_BIN)

# === Cobertura ===
coverage: CFLAGS += -O0 -g --coverage
coverage: clean
	$(CC) $(CFLAGS) -o $(TEST_BIN) $(TESTS) $(SRC)
	./$(TEST_BIN)
	@echo ""
	@echo "=== gcov por función ==="
	gcov -f -b src/ring_buffer.c
	@echo ""
	@echo "=== lcov HTML ==="
	lcov --capture --initial -d . -o base.info -q
	lcov --capture -d . -o test.info -q
	lcov -a base.info -a test.info -o total.info -q
	lcov --remove total.info '/usr/*' '*/tests/*' -o filtered.info -q
	genhtml filtered.info -o $(COV_DIR) \
		--title "Ring Buffer" --legend --branch-coverage -q
	@echo ""
	lcov --summary filtered.info
	@echo ""
	@echo "Reporte: $(COV_DIR)/index.html"

clean:
	rm -f $(TEST_BIN) *.gcno *.gcda *.gcov *.info
	rm -f src/*.gcno src/*.gcda
	rm -rf $(COV_DIR)
```

### Ejecución y análisis del reporte

```bash
$ make coverage
gcc -Wall -Wextra -std=c11 -Isrc -O0 -g --coverage -o test_ring_buffer \
    tests/test_ring_buffer.c src/ring_buffer.c
./test_ring_buffer
=== Ring Buffer Tests ===
  test_create_normal                           PASS
  test_create_zero_capacity                    PASS
  test_push_pop_single                         PASS
  test_push_pop_multiple                       PASS
  test_peek                                    PASS
  test_clear                                   PASS

6 tests passed

=== gcov por función ===
Function 'ring_buffer_create'
Lines executed:100.00% of 10

Function 'ring_buffer_destroy'
Lines executed:100.00% of 3

Function 'ring_buffer_push'
Lines executed:72.73% of 11

Function 'ring_buffer_pop'
Lines executed:83.33% of 6

Function 'ring_buffer_peek'
Lines executed:66.67% of 6

Function 'ring_buffer_write'
Lines executed:0.00% of 6

Function 'ring_buffer_read'
Lines executed:0.00% of 6

Function 'ring_buffer_count'
Lines executed:66.67% of 3

Function 'ring_buffer_free_space'
Lines executed:66.67% of 3

Function 'ring_buffer_is_full'
Lines executed:66.67% of 3

Function 'ring_buffer_is_empty'
Lines executed:66.67% of 3

Function 'ring_buffer_clear'
Lines executed:75.00% of 4

Function 'ring_buffer_dump'
Lines executed:0.00% of 14

File 'src/ring_buffer.c'
Lines executed:54.79% of 78
Branches executed:60.00% of 40
Taken at least once:42.50% of 40

=== lcov HTML ===

Summary coverage rate:
  lines......: 54.8% (40 of 73)
  functions..: 76.9% (10 of 13)
  branches...: 42.5% (17 of 40)

Reporte: coverage_html/index.html
```

### Mapa visual de cobertura

```
  ring_buffer.c — Cobertura: 54.8% líneas, 42.5% ramas

  ring_buffer_create     ████████████████████  100%  ✓ Completa
  ring_buffer_destroy    ████████████████████  100%  ✓ Completa
  ring_buffer_push       ██████████████░░░░░░   73%  ✗ Falta: overwrite, NULL
  ring_buffer_pop        ████████████████░░░░   83%  ✗ Falta: NULL, data=NULL
  ring_buffer_peek       █████████████░░░░░░░   67%  ✗ Falta: vacío, NULL
  ring_buffer_write      ░░░░░░░░░░░░░░░░░░░░    0%  ✗ SIN TESTS
  ring_buffer_read       ░░░░░░░░░░░░░░░░░░░░    0%  ✗ SIN TESTS
  ring_buffer_count      █████████████░░░░░░░   67%  ✗ Falta: NULL
  ring_buffer_free_space █████████████░░░░░░░   67%  ✗ Falta: NULL
  ring_buffer_is_full    █████████████░░░░░░░   67%  ✗ Falta: NULL
  ring_buffer_is_empty   █████████████░░░░░░░   67%  ✗ Falta: NULL
  ring_buffer_clear      ███████████████░░░░░   75%  ✗ Falta: NULL
  ring_buffer_dump       ░░░░░░░░░░░░░░░░░░░░    0%  ✗ SIN TESTS (debug)

  Prioridades de testing:
  1. ring_buffer_write y ring_buffer_read (0%)
  2. Modo overwrite en push (rama no cubierta)
  3. Wraparound circular (push/pop con wrap)
  4. Casos NULL en todas las funciones
  5. ring_buffer_dump (debug, baja prioridad)
```

### Lo que el reporte de ramas revela

```
  ring_buffer_push — Ramas:

   if (!rb)                      branch 0: never executed  ← nunca NULL
                                 branch 1: taken 9 times   ← siempre válido

   if (count == capacity)        branch 0: taken 1 time    ← lleno 1 vez
                                 branch 1: taken 8 times   ← no lleno 8 veces

   if (!rb->overwrite)           branch 0: taken 1 time    ← overwrite=false
                                 branch 1: never executed   ← overwrite=true NUNCA

  Conclusión: El modo overwrite jamás se ejercitó.
  El test test_push_pop_multiple sí llena el buffer, pero con overwrite=false.
```

---

## 21. Programa de práctica

### Enunciado

Toma el proyecto **ring_buffer** de la sección 20 y:

1. Ejecuta el flujo completo de cobertura: compilar con `--coverage`, ejecutar tests, generar reporte con gcov y lcov+genhtml

2. Analiza el reporte e identifica:
   - Funciones con 0% de cobertura
   - Ramas no cubiertas en funciones parcialmente cubiertas
   - El código muerto real vs código defensivo (guards `if (!rb)`)

3. Añade tests hasta alcanzar al menos **90% de cobertura de líneas** y **75% de cobertura de ramas** (sin contar `ring_buffer_dump`):
   - Tests para `ring_buffer_write` y `ring_buffer_read`
   - Tests para modo overwrite (`overwrite=true`)
   - Tests para wraparound circular
   - Tests para argumentos NULL
   - Tests para `ring_buffer_peek` en buffer vacío

4. Verifica con `lcov --summary` que se alcanzaron los umbrales

5. (Opcional) Compara el reporte de cobertura antes y después: genera dos reportes HTML separados

### Pistas

```
# Paso 1: Compilar y ver cobertura inicial
make coverage

# Paso 2: Abrir y analizar
xdg-open coverage_html/index.html

# Paso 3: Añadir tests a tests/test_ring_buffer.c
# Después de cada grupo de tests, re-ejecutar:
make coverage

# Paso 4: Verificar
lcov --summary filtered.info

# Paso 5: Para comparar antes/después
cp -r coverage_html coverage_html_before
# Añadir tests, recompilar
make coverage
# Ahora tienes coverage_html (después) y coverage_html_before (antes)
```

---

## 22. Ejercicios

### Ejercicio 1: gcov básico (20 min)

Dado el siguiente código:

```c
// temperature.c
#include <stdio.h>
#include <stdbool.h>

typedef enum { CELSIUS, FAHRENHEIT, KELVIN } TempUnit;

typedef struct {
    double value;
    TempUnit unit;
} Temperature;

double to_celsius(Temperature t) {
    switch (t.unit) {
        case CELSIUS:    return t.value;
        case FAHRENHEIT: return (t.value - 32.0) * 5.0 / 9.0;
        case KELVIN:     return t.value - 273.15;
        default:         return 0.0;
    }
}

const char *classify_temp(double celsius) {
    if (celsius < -40.0)   return "extreme cold";
    if (celsius < 0.0)     return "freezing";
    if (celsius < 15.0)    return "cold";
    if (celsius < 25.0)    return "comfortable";
    if (celsius < 35.0)    return "warm";
    if (celsius < 45.0)    return "hot";
    return "extreme heat";
}

bool is_valid_temp(Temperature t) {
    double c = to_celsius(t);
    if (c < -273.15) return false;  // Below absolute zero
    if (c > 1000.0)  return false;  // Unreasonable
    return true;
}

void print_temp_report(Temperature t) {
    if (!is_valid_temp(t)) {
        printf("Invalid temperature\n");
        return;
    }
    double c = to_celsius(t);
    printf("%.1f C (%s)\n", c, classify_temp(c));
}
```

**Tareas**:
- a) Escribe un `test_temperature.c` con assert() que alcance **al menos 60%** de line coverage
- b) Compila con `--coverage`, ejecuta, y analiza el reporte con `gcov -f -b`
- c) Identifica qué ramas específicas no están cubiertas
- d) Añade tests para alcanzar **100% de line coverage** y al menos **80% de branch coverage**

### Ejercicio 2: lcov y genhtml (25 min)

**Tareas**:
- a) Toma el ejercicio 1 y genera un reporte HTML completo con lcov+genhtml
- b) Usa `lcov --remove` para excluir el archivo de test del reporte
- c) Genera dos reportes HTML:
  - Uno con solo los tests iniciales (60%)
  - Otro con todos los tests (100%)
- d) Compara visualmente ambos reportes y documenta las diferencias
- e) Configura un `.lcovrc` que active branch coverage por defecto y ponga umbrales en 85/70

### Ejercicio 3: Makefile con cobertura (25 min)

Crea un proyecto con la siguiente estructura:

```
matrix/
├── src/
│   ├── matrix.h     ← Operaciones con matrices: create, destroy, add, multiply, transpose
│   └── matrix.c
├── tests/
│   ├── test_create.c
│   ├── test_operations.c
│   └── test_edge_cases.c
└── Makefile
```

**Tareas**:
- a) Implementa la biblioteca `matrix` (al menos create, destroy, add, multiply, transpose)
- b) Escribe tests divididos en los tres archivos indicados
- c) Crea un `Makefile` con targets:
  - `make test` — compilar y ejecutar tests sin cobertura
  - `make coverage` — compilar con cobertura, ejecutar, generar HTML
  - `make coverage-summary` — solo resumen numérico
  - `make coverage-check COV_THRESHOLD=80` — falla si cobertura < umbral
  - `make clean`
- d) Alcanza al menos 85% de line coverage y 70% de branch coverage

### Ejercicio 4: gcovr y formatos de CI (20 min)

**Tareas**:
- a) Instala gcovr (`pip install gcovr`)
- b) Usando el proyecto del ejercicio 3, genera reportes en los siguientes formatos:
  - Terminal (texto)
  - HTML detallado
  - JSON
  - Cobertura XML (formato Cobertura)
- c) Usa `gcovr --fail-under-line 80 --fail-under-branch 60` y verifica que pasa
- d) Reduce temporalmente los tests para que falle con `--fail-under-line 90`
- e) Compara la experiencia gcovr vs lcov+genhtml: número de comandos, formatos soportados, facilidad de uso

---

**Navegación**:
- ← Anterior: [S03/T04 — CMocka mocking](../../S03_Mocking_y_Test_Doubles_en_C/T04_CMocka_mocking/README.md)
- → Siguiente: [T02 — Interpretar cobertura](../T02_Interpretar_cobertura/README.md)
- ↑ Sección: [S04 — Cobertura en C](../README.md)
