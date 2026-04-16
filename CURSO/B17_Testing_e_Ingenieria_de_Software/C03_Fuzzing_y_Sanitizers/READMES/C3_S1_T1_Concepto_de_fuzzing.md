# T01 - Concepto de fuzzing: encontrar bugs que los humanos no imaginan

## Índice

1. [Qué es fuzzing](#1-qué-es-fuzzing)
2. [El principio fundamental: inputs inesperados](#2-el-principio-fundamental-inputs-inesperados)
3. [Historia breve del fuzzing](#3-historia-breve-del-fuzzing)
4. [Fuzzing vs testing tradicional](#4-fuzzing-vs-testing-tradicional)
5. [Generación de inputs: el corazón del fuzzer](#5-generación-de-inputs-el-corazón-del-fuzzer)
6. [Dumb fuzzing (generación aleatoria)](#6-dumb-fuzzing-generación-aleatoria)
7. [Mutation-based fuzzing](#7-mutation-based-fuzzing)
8. [Generation-based fuzzing](#8-generation-based-fuzzing)
9. [Coverage-guided fuzzing: la revolución](#9-coverage-guided-fuzzing-la-revolución)
10. [Instrumentación: cómo el fuzzer "ve" el código](#10-instrumentación-cómo-el-fuzzer-ve-el-código)
11. [El corpus: la memoria del fuzzer](#11-el-corpus-la-memoria-del-fuzzer)
12. [Corpus inicial: semillas que aceleran la exploración](#12-corpus-inicial-semillas-que-aceleran-la-exploración)
13. [Corpus minimization](#13-corpus-minimization)
14. [Crash discovery: qué busca el fuzzer](#14-crash-discovery-qué-busca-el-fuzzer)
15. [Tipos de bugs que encuentra el fuzzing](#15-tipos-de-bugs-que-encuentra-el-fuzzing)
16. [El harness: la función target](#16-el-harness-la-función-target)
17. [Anatomía de una sesión de fuzzing](#17-anatomía-de-una-sesión-de-fuzzing)
18. [Métricas de una sesión de fuzzing](#18-métricas-de-una-sesión-de-fuzzing)
19. [Diccionarios: ayudar al fuzzer con tokens](#19-diccionarios-ayudar-al-fuzzer-con-tokens)
20. [Sanitizers: amplificar la detección](#20-sanitizers-amplificar-la-detección)
21. [Fuzzing determinista vs no determinista](#21-fuzzing-determinista-vs-no-determinista)
22. [Reproducibilidad: del crash al test case](#22-reproducibilidad-del-crash-al-test-case)
23. [Cuándo usar fuzzing (y cuándo no)](#23-cuándo-usar-fuzzing-y-cuándo-no)
24. [El ecosistema de fuzzers](#24-el-ecosistema-de-fuzzers)
25. [Fuzzing en C vs Rust vs Go](#25-fuzzing-en-c-vs-rust-vs-go)
26. [Errores comunes al empezar con fuzzing](#26-errores-comunes-al-empezar-con-fuzzing)
27. [Ejemplo completo: fuzzear un parser de CSV en C](#27-ejemplo-completo-fuzzear-un-parser-de-csv-en-c)
28. [Programa de práctica: mini_json_parser](#28-programa-de-práctica-mini_json_parser)
29. [Ejercicios](#29-ejercicios)

---

## 1. Qué es fuzzing

Fuzzing (o fuzz testing) es una técnica de testing automatizado que consiste en
alimentar un programa con inputs generados automáticamente — masivos, inesperados,
malformados — y observar si el programa se comporta incorrectamente.

```
┌──────────────────────────────────────────────────────────────────┐
│                    ¿Qué es fuzzing?                              │
│                                                                  │
│                   ┌────────────┐                                  │
│   Inputs ────────▶│  Programa  │────────▶ Observar               │
│   generados       │  bajo test │         comportamiento           │
│   automática-     └────────────┘                                  │
│   mente                                                          │
│                                                                  │
│   El fuzzer genera millones de inputs por segundo.               │
│   El programa se ejecuta con cada uno.                           │
│   Si el programa:                                                │
│   • Crashea (segfault, abort, panic) → BUG ENCONTRADO            │
│   • Se cuelga (timeout)             → BUG ENCONTRADO             │
│   • Consume memoria sin límite      → BUG ENCONTRADO             │
│   • Viola un assert/sanitizer       → BUG ENCONTRADO             │
│   • Retorna normalmente             → Siguiente input            │
│                                                                  │
│   La idea: si pruebas suficientes inputs raros, encontrarás      │
│   los que rompen las suposiciones del código.                    │
└──────────────────────────────────────────────────────────────────┘
```

La diferencia fundamental con testing tradicional:

| Testing tradicional | Fuzzing |
|---|---|
| El humano escribe cada input | La máquina genera los inputs |
| Decenas a miles de tests | Millones a billones de inputs |
| Verifica comportamiento esperado | Busca comportamiento inesperado |
| "¿Hace lo correcto?" | "¿Se rompe con algo?" |
| Determinista | Estocástico (con guía) |
| Limitado por la imaginación humana | Limitado solo por el tiempo |

---

## 2. El principio fundamental: inputs inesperados

Los bugs más peligrosos no están en los caminos que el programador anticipó. Están
en los caminos que nadie imaginó:

```
┌──────────────────────────────────────────────────────────────────────┐
│         Los inputs que los humanos NO imaginan                       │
│                                                                      │
│  Función: parse_age(const char *input)                               │
│                                                                      │
│  Lo que el programador testea:                                       │
│  ├── "25"          → 25 ✓                                            │
│  ├── "0"           → 0 ✓                                             │
│  ├── "120"         → 120 ✓                                           │
│  ├── "-1"          → error ✓                                         │
│  ├── "abc"         → error ✓                                         │
│  └── ""            → error ✓                                         │
│                                                                      │
│  Lo que el fuzzer encuentra:                                         │
│  ├── "999999999999999999999"  → integer overflow → crash             │
│  ├── "25\x00hidden"          → null byte confunde strlen             │
│  ├── "  \t\n 25 \r\n  "     → whitespace no manejado                │
│  ├── "-0"                    → ¿es válido? comportamiento ambiguo    │
│  ├── "+25"                   → ¿acepta el signo +?                   │
│  ├── "0000000025"            → leading zeros → octal?                │
│  ├── "25e2"                  → notación científica → 2500?           │
│  ├── "0x19"                  → hexadecimal → 25?                     │
│  ├── "\xff\xfe25"            → BOM de Unicode                        │
│  ├── "25" (10000 veces)      → string de 50000 bytes → buffer?       │
│  └── "\x00"                  → solo un null byte                     │
│                                                                      │
│  El fuzzer no "piensa" en estos casos — los descubre                 │
│  mecánicamente probando combinaciones.                               │
└──────────────────────────────────────────────────────────────────────┘
```

### Por qué los humanos fallan

Los programadores tienen **sesgos cognitivos** al escribir tests:

1. **Sesgo de confirmación**: escribimos tests que confirman que el código funciona,
   no tests que intenten romperlo
2. **Sesgo de normalidad**: testeamos inputs "normales" porque son los que imaginamos
3. **Punto ciego del autor**: el que escribe el código no puede imaginar los usos
   incorrectos porque conoce demasiado bien la intención correcta
4. **Fatiga combinatoria**: un parser con 10 reglas tiene miles de combinaciones
   posibles — nadie las testea todas manualmente

El fuzzing no tiene estos sesgos. Es pura exploración mecánica.

---

## 3. Historia breve del fuzzing

```
┌──────────────────────────────────────────────────────────────────────┐
│                  Línea de tiempo del fuzzing                         │
│                                                                      │
│  1988  Barton Miller (U. Wisconsin)                                  │
│  │     "An Empirical Study of the Reliability of UNIX Utilities"     │
│  │     Envió inputs aleatorios a utilidades UNIX.                    │
│  │     Resultado: 25-33% de los programas crashearon.                │
│  │     Nacimiento del término "fuzz testing".                        │
│  │                                                                   │
│  1999  PROTOS project (U. Oulu, Finlandia)                           │
│  │     Fuzzing de protocolos de red (SNMP, SIP, LDAP).               │
│  │     Encontró vulnerabilidades en casi todas las implementaciones.  │
│  │                                                                   │
│  2004  Peach Fuzzer                                                  │
│  │     Primer fuzzer comercial con formato de datos modelable.       │
│  │     Generation-based fuzzing con gramáticas XML.                  │
│  │                                                                   │
│  2007  AFL (American Fuzzy Lop) - Michał Zalewski                   │
│  │     Revolución: coverage-guided fuzzing con instrumentación.      │
│  │     El fuzzer "aprende" qué inputs exploran código nuevo.         │
│  │     Encontró bugs en TODOS los formatos de imagen, parsers,       │
│  │     compresores, etc.                                             │
│  │                                                                   │
│  2015  libFuzzer (LLVM project)                                      │
│  │     In-process fuzzing: el target se compila con el fuzzer.       │
│  │     Más rápido que AFL (no hay fork/exec por input).              │
│  │     Se integra con sanitizers (ASan, MSan, UBSan).                │
│  │                                                                   │
│  2016  OSS-Fuzz (Google)                                             │
│  │     Fuzzing continuo para proyectos open source.                  │
│  │     Ha encontrado 10,000+ bugs en 1,000+ proyectos.              │
│  │                                                                   │
│  2019  AFL++ (fork mejorado de AFL)                                  │
│  │     Mejor mutación, más instrumentación, modo persistente.        │
│  │     Se convierte en el estándar de facto.                         │
│  │                                                                   │
│  2020  Go fuzzing nativo (go test -fuzz)                             │
│  │     Primer lenguaje con fuzzing integrado en su toolchain.        │
│  │                                                                   │
│  2022  Rust: cargo-fuzz maduro, libFuzzer backend                    │
│        Fuzzing de Rust con Arbitrary trait para inputs estructurados. │
└──────────────────────────────────────────────────────────────────────┘
```

### El antes y el después: AFL

Antes de AFL (2007), el fuzzing era principalmente "dumb" — inputs aleatorios sin
retroalimentación. AFL cambió todo al introducir **coverage-guided fuzzing**: el
fuzzer observa qué caminos del código ejecuta cada input y favorece los inputs que
descubren caminos nuevos. Esto convirtió el fuzzing de una técnica artesanal en una
herramienta industrial.

---

## 4. Fuzzing vs testing tradicional

```
┌──────────────────────────────────────────────────────────────────────┐
│          Fuzzing vs Testing: No se reemplazan                        │
│                                                                      │
│                    Testing unitario                                  │
│                    ┌───────────┐                                     │
│                    │ Verifica  │     Fuzzing                         │
│                    │ que el    │     ┌────────────┐                  │
│                    │ programa  │     │ Busca lo   │                  │
│                    │ HACE lo   │     │ que el     │                  │
│                    │ correcto  │     │ programa   │                  │
│                    └───────────┘     │ NO debería │                  │
│                         │           │ hacer      │                  │
│                         │           └────────────┘                  │
│                         │                │                          │
│                         ▼                ▼                          │
│                    ┌──────────────────────────┐                      │
│                    │    Confianza completa     │                     │
│                    │    en el programa         │                     │
│                    └──────────────────────────┘                      │
│                                                                      │
│  Son complementarios:                                                │
│  • Unit tests: "add(2, 3) == 5" ← corrección funcional              │
│  • Fuzzing: "parse(random_bytes) no crashea" ← robustez             │
│                                                                      │
│  Analogía:                                                           │
│  • Unit tests = examen médico planificado                            │
│  • Fuzzing = stress test cardiológico                                │
└──────────────────────────────────────────────────────────────────────┘
```

### Comparación detallada

| Aspecto | Unit/Integration tests | Property-based testing | Fuzzing |
|---|---|---|---|
| **Quién escribe inputs** | El programador | Framework (con constraints) | El fuzzer (libre) |
| **Cantidad de inputs** | 10-1000 | 100-10000 | Millones-billones |
| **Tipos de bugs** | Lógicos, de contrato | Violaciones de propiedades | Crashes, memory bugs, hangs |
| **Determinismo** | 100% determinista | Determinista con seed | No determinista |
| **Tiempo de ejecución** | Segundos | Segundos-minutos | Minutos-horas-días |
| **Esfuerzo de setup** | Medio | Medio | Bajo (harness simple) |
| **Mantenimiento** | Alto (actualizar tests) | Medio | Bajo (el harness cambia poco) |
| **Falsos positivos** | Nunca | Raramente | Nunca (si crashea, es bug) |
| **Cobertura de edge cases** | Manual | Parcial | Excelente |
| **Lenguajes** | Todos | Todos (con framework) | C, C++, Rust, Go (nativamente) |

### Cuándo cada técnica brilla

```
  Unit tests          Property tests         Fuzzing
  ──────────          ──────────────         ───────
  Lógica de           Invariantes            Parsers
  negocio             matemáticas            Deserializadores
  
  APIs con            Codecs (encode/        Procesamiento de
  contrato claro      decode roundtrip)      archivos
  
  Regresiones         Transformaciones       Protocolos de red
  conocidas           reversibles            
  
  Integración         Ordenación,            Código con unsafe
  de componentes      búsqueda               (C, Rust unsafe)
```

---

## 5. Generación de inputs: el corazón del fuzzer

La calidad de un fuzzer depende directamente de cómo genera sus inputs. Hay cuatro
estrategias principales:

```
┌──────────────────────────────────────────────────────────────────────┐
│           Estrategias de generación de inputs                        │
│                                                                      │
│  1. DUMB (aleatorio puro)                                           │
│     ████████████████████ → bytes aleatorios                         │
│     Simple pero ineficiente                                          │
│                                                                      │
│  2. MUTATION-BASED                                                   │
│     seed ──▶ [mutar] ──▶ variante ──▶ [mutar] ──▶ variante          │
│     Parte de inputs válidos y los modifica                           │
│                                                                      │
│  3. GENERATION-BASED                                                 │
│     gramática ──▶ [generar] ──▶ input válido                        │
│     Genera inputs que cumplen un formato                             │
│                                                                      │
│  4. COVERAGE-GUIDED (mutation + feedback)                            │
│     input ──▶ [ejecutar] ──▶ ¿nuevo camino?                        │
│                                  │         │                        │
│                                 Sí        No                        │
│                                  │         │                        │
│                                  ▼         ▼                        │
│                              guardar    descartar                    │
│                              en corpus                               │
│                                  │                                  │
│                                  ▼                                  │
│                              [mutar] ──▶ siguiente iteración         │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 6. Dumb fuzzing (generación aleatoria)

El fuzzing más simple: generar bytes completamente aleatorios y pasarlos al programa.

### Implementación conceptual en C

```c
/* dumb_fuzzer.c — el fuzzer más simple posible */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

/* Genera un archivo con bytes aleatorios */
void generate_random_file(const char *path, size_t max_size) {
    size_t size = rand() % max_size + 1;
    FILE *f = fopen(path, "wb");
    for (size_t i = 0; i < size; i++) {
        fputc(rand() % 256, f);
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program_to_fuzz>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    const char *target = argv[1];
    const char *tmp_input = "/tmp/fuzz_input";
    int iterations = 0;
    int crashes = 0;

    while (1) {
        generate_random_file(tmp_input, 4096);
        iterations++;

        pid_t pid = fork();
        if (pid == 0) {
            /* Proceso hijo: ejecutar el programa con el input */
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            execl(target, target, tmp_input, NULL);
            _exit(1);
        }

        int status;
        waitpid(pid, &status, 0);

        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            crashes++;
            /* Guardar el input que causó el crash */
            char crash_file[256];
            snprintf(crash_file, sizeof(crash_file),
                     "crash_%d_sig%d", iterations, sig);
            /* copiar tmp_input a crash_file */
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "cp %s %s", tmp_input, crash_file);
            system(cmd);
            printf("[!] Crash #%d at iteration %d (signal %d) → %s\n",
                   crashes, iterations, sig, crash_file);
        }

        if (iterations % 10000 == 0) {
            printf("[*] %d iterations, %d crashes\n", iterations, crashes);
        }
    }
}
```

### Problemas del dumb fuzzing

```
┌──────────────────────────────────────────────────────────────────────┐
│            Por qué el dumb fuzzing es ineficiente                    │
│                                                                      │
│  Programa target: parser de PNG                                      │
│                                                                      │
│  Un PNG válido empieza con: 89 50 4E 47 0D 0A 1A 0A                │
│                              (8 bytes mágicos exactos)               │
│                                                                      │
│  Probabilidad de que bytes aleatorios empiecen con esos 8 bytes:     │
│  (1/256)^8 = 1 en 18,446,744,073,709,551,616                       │
│                                                                      │
│  Es decir: el fuzzer generaría ~18 quintillones de inputs           │
│  antes de producir UNO que pase el primer if del parser.            │
│                                                                      │
│  El 99.99999...% de los inputs serían rechazados en la              │
│  primera línea:                                                      │
│                                                                      │
│  if (memcmp(header, PNG_MAGIC, 8) != 0) {                           │
│      return ERR_NOT_PNG;  ← aquí muere todo                        │
│  }                                                                   │
│  /* El fuzzer NUNCA llega aquí */                                    │
│  parse_chunks(data);  ← donde están los bugs reales                 │
│                                                                      │
│  Conclusión: dumb fuzzing solo funciona para programas que           │
│  aceptan cualquier input (sin formato rígido).                       │
└──────────────────────────────────────────────────────────────────────┘
```

### Cuándo el dumb fuzzing SÍ funciona

| Escenario | Por qué funciona |
|---|---|
| Parsers de texto libre | No hay "magic bytes" que validar |
| Funciones de string (strlen, strcmp) | Cualquier byte es válido |
| Funciones matemáticas con input numérico | Los bytes se interpretan como números |
| Programas sin validación de formato | Todo input se procesa |

---

## 7. Mutation-based fuzzing

La idea: partir de inputs **válidos** (semillas/seeds) y modificarlos ligeramente
para producir variantes que exploren caminos cercanos.

### Operaciones de mutación

```
┌──────────────────────────────────────────────────────────────────────┐
│            Operaciones de mutación típicas                           │
│                                                                      │
│  Input original (semilla):                                           │
│  [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]  ("Hello, World")           │
│                                                                      │
│  1. BIT FLIP: invertir un bit aleatorio                              │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│      ↓                                                               │
│     [49 65 6C 6C 6F 2C 20 57 6F 72 6C 64]  ("Iello, World")        │
│      ^^ un bit cambió                                                │
│                                                                      │
│  2. BYTE FLIP: cambiar un byte completo                              │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│                              ↓                                       │
│     [48 65 6C 6C 6F 2C 20 FF 6F 72 6C 64]  byte 7 → 0xFF           │
│                                                                      │
│  3. BYTE INSERTION: insertar un byte                                 │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│                        ↓                                             │
│     [48 65 6C 6C 6F 2C 00 20 57 6F 72 6C 64]  null insertado       │
│                                                                      │
│  4. BYTE DELETION: eliminar un byte                                  │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│                     ↓                                                │
│     [48 65 6C 6C 2C 20 57 6F 72 6C 64]  'o' eliminado              │
│                                                                      │
│  5. BLOCK COPY: copiar un bloque a otra posición                     │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│     [48 65 6C 6C 6F 2C 6C 6C 6F 72 6C 64]  "ll" copiado            │
│                                                                      │
│  6. INTERESTING VALUES: sustituir con valores especiales             │
│     0, 1, -1, 127, 128, 255, 256, 32767, 32768, 65535,              │
│     2147483647, -2147483648, INT_MAX, INT_MIN, etc.                  │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│      ↓↓↓↓                                                           │
│     [FF FF FF FF 6F 2C 20 57 6F 72 6C 64]  bytes 0-3 → 0xFFFFFFFF  │
│                                                                      │
│  7. ARITHMETIC: sumar/restar valores pequeños a bytes                │
│     [48 65 6C 6C 6F 2C 20 57 6F 72 6C 64]                          │
│      ↓                                                               │
│     [49 65 6C 6C 6F 2C 20 57 6F 72 6C 64]  byte 0 += 1             │
│                                                                      │
│  8. HAVOC: combinar múltiples mutaciones aleatorias                  │
│     (la estrategia más efectiva de AFL)                              │
└──────────────────────────────────────────────────────────────────────┘
```

### Ventaja sobre dumb fuzzing

```
  Dumb fuzzing con parser PNG:
  
  Input aleatorio → [7B A2 1F ...]
                     ↓
  if (header != PNG_MAGIC) return ERR;  ← rechazado inmediatamente
  
  Mutation-based fuzzing con parser PNG:
  
  Semilla: archivo PNG real → [89 50 4E 47 0D 0A 1A 0A ...]
                               ↓ (mutar byte 9)
  Mutación: [89 50 4E 47 0D 0A 1A 0A FF ...]
             ↓
  if (header == PNG_MAGIC) → PASA ✓
  parse_chunks() → ejecuta código profundo del parser
                   → puede encontrar bugs reales
```

La mutación preserva la estructura general del input (los magic bytes, headers,
campos de longitud) mientras altera los detalles. Esto permite que el fuzzer
penetre las validaciones iniciales y llegue al código interesante.

---

## 8. Generation-based fuzzing

En lugar de mutar inputs existentes, se generan inputs desde cero siguiendo una
gramática o especificación del formato.

### Ejemplo: gramática de JSON simplificado

```
value    := object | array | string | number | "true" | "false" | "null"
object   := "{" (pair ("," pair)*)? "}"
pair     := string ":" value
array    := "[" (value ("," value)*)? "]"
string   := '"' char* '"'
number   := "-"? digit+ ("." digit+)?
```

Un fuzzer basado en gramática genera inputs como:

```json
{"a":[true,null,{"b":999999999999.999}],"c":[[[]]]}
```

Cada input es **sintácticamente válido** según la gramática, lo que garantiza que
el parser no lo rechace inmediatamente.

### Ventajas y desventajas

```
┌──────────────────────────────────────────────────────────────────┐
│          Generation-based vs Mutation-based                      │
│                                                                  │
│  Generation-based:                                               │
│  ✓ Inputs siempre válidos sintácticamente                        │
│  ✓ Explora estructuras complejas desde el inicio                 │
│  ✗ Requiere escribir la gramática (esfuerzo alto)                │
│  ✗ No descubre bugs en el parser de sintaxis                     │
│  ✗ Limitado a lo que la gramática permite                        │
│                                                                  │
│  Mutation-based:                                                 │
│  ✓ No necesita conocer el formato                                │
│  ✓ Encuentra bugs en el parser de sintaxis                       │
│  ✓ Descubre inputs inesperados                                   │
│  ✗ Necesita semillas iniciales buenas                             │
│  ✗ Puede tardar en encontrar formatos complejos                  │
│                                                                  │
│  En la práctica, mutation-based + coverage-guided (AFL, libFuzzer)│
│  ha demostrado ser superior en la mayoría de los casos.          │
└──────────────────────────────────────────────────────────────────┘
```

---

## 9. Coverage-guided fuzzing: la revolución

El coverage-guided fuzzing combina mutation-based fuzzing con **retroalimentación
de cobertura de código**. El fuzzer observa qué caminos del código ejecuta cada
input y favorece los inputs que descubren caminos nuevos.

### El algoritmo

```
┌──────────────────────────────────────────────────────────────────────┐
│           Algoritmo de coverage-guided fuzzing                       │
│                                                                      │
│  1. INICIALIZAR                                                      │
│     corpus = cargar_semillas(directorio_corpus)                      │
│     if corpus está vacío:                                            │
│         corpus = [bytes_aleatorios_pequeños]                         │
│     mapa_cobertura_global = {}                                       │
│                                                                      │
│  2. LOOP PRINCIPAL (repetir indefinidamente)                         │
│     │                                                                │
│     ├── a) Elegir un input del corpus (por energía/prioridad)        │
│     │                                                                │
│     ├── b) Mutar el input (bit flip, byte insert, havoc, etc.)       │
│     │                                                                │
│     ├── c) Ejecutar el programa con el input mutado                  │
│     │      └── Recoger el mapa de cobertura de ESTA ejecución        │
│     │                                                                │
│     ├── d) Evaluar:                                                  │
│     │      ├── ¿Crash/timeout? → guardar en crashes/                 │
│     │      │                                                         │
│     │      ├── ¿Descubrió edges/bloques nuevos?                     │
│     │      │   (comparar con mapa_cobertura_global)                  │
│     │      │   │                                                     │
│     │      │   ├── SÍ → agregar al corpus                           │
│     │      │   │        actualizar mapa_cobertura_global             │
│     │      │   │        este input es "interesante"                  │
│     │      │   │                                                     │
│     │      │   └── NO → descartar el input                          │
│     │      │            (no aporta cobertura nueva)                  │
│     │      │                                                         │
│     │      └── Actualizar estadísticas                               │
│     │                                                                │
│     └── e) Volver a (a)                                              │
│                                                                      │
│  La clave: el corpus CRECE con inputs que descubren código nuevo.    │
│  Con el tiempo, el fuzzer explora cada vez más profundo.             │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo visual

```
  Iteración 1: ejecutar semilla original
  Código cubierto: [A, B, C]
  
  Iteración 2: mutar semilla → input_2
  Código cubierto: [A, B, C]         ← nada nuevo, descartar
  
  Iteración 3: mutar semilla → input_3
  Código cubierto: [A, B, C, D, E]   ← ¡nuevo! D y E
  Agregar input_3 al corpus
  
  Iteración 4: mutar input_3 → input_4
  Código cubierto: [A, B, D, E, F]   ← ¡nuevo! F
  Agregar input_4 al corpus
  
  Iteración 5: mutar input_4 → input_5
  Código cubierto: [A, B, D, E, F, G, H] → ¡nuevo! G y H
  Agregar input_5 al corpus
  
  ...y así sucesivamente, explorando cada vez más código.
  
  Cobertura a lo largo del tiempo:
  
  100%│                                    ╱────────────
      │                                 ╱╱
      │                              ╱╱
      │                           ╱╱
      │                       ╱╱
      │                   ╱╱
      │               ╱╱
      │          ╱╱╱╱
      │     ╱╱╱╱
      │╱╱╱╱
   0% └────────────────────────────────────────────────
      0         Tiempo de fuzzing →
  
  La curva se aplana: cada nuevo camino es más difícil de encontrar.
```

### Por qué funciona tan bien

El coverage-guided fuzzing es una forma de **algoritmo evolutivo**:

1. **Selección**: inputs que descubren código nuevo son "más aptos"
2. **Mutación**: variantes aleatorias de los inputs seleccionados
3. **Fitness**: la cobertura de código es la función de fitness
4. **Población**: el corpus crece con los individuos más aptos

Con suficiente tiempo, el fuzzer "evoluciona" inputs que navegan caminos
cada vez más profundos y complejos del código.

---

## 10. Instrumentación: cómo el fuzzer "ve" el código

Para que el fuzzer sepa qué caminos ejecuta cada input, necesita **instrumentar**
el código — insertar contadores en puntos estratégicos.

### Qué se instrumenta

```
┌──────────────────────────────────────────────────────────────────────┐
│             Instrumentación para fuzzing                             │
│                                                                      │
│  Código original:                                                    │
│                                                                      │
│  int parse(const char *s) {                                          │
│      if (s[0] == 'A') {          // branch 1                        │
│          if (s[1] == 'B') {      // branch 2                        │
│              return handle_ab(s); // bloque 3                        │
│          }                                                           │
│          return handle_a(s);      // bloque 4                        │
│      }                                                               │
│      return handle_other(s);      // bloque 5                        │
│  }                                                                   │
│                                                                      │
│  Código instrumentado (conceptualmente):                             │
│                                                                      │
│  int parse(const char *s) {                                          │
│      __coverage_trace_edge(0, 1);  // entrar a la función           │
│      if (s[0] == 'A') {                                              │
│          __coverage_trace_edge(1, 2);  // branch 1 → true           │
│          if (s[1] == 'B') {                                          │
│              __coverage_trace_edge(2, 3);  // branch 2 → true       │
│              return handle_ab(s);                                    │
│          }                                                           │
│          __coverage_trace_edge(2, 4);  // branch 2 → false          │
│          return handle_a(s);                                         │
│      }                                                               │
│      __coverage_trace_edge(1, 5);  // branch 1 → false              │
│      return handle_other(s);                                         │
│  }                                                                   │
│                                                                      │
│  El fuzzer mantiene un mapa de edges: {(0,1):hit, (1,2):hit, ...}   │
│  Cada ejecución produce un "fingerprint" de cobertura.               │
│  Si el fingerprint es nuevo → input interesante.                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Niveles de instrumentación

| Nivel | Qué mide | Herramienta | Resolución |
|---|---|---|---|
| **Basic block** | Si cada bloque se ejecutó (sí/no) | AFL básico | Baja |
| **Edge coverage** | Transiciones entre bloques (A→B, A→C) | AFL++, libFuzzer | Media |
| **Hit counts** | Cuántas veces se ejecutó cada edge | AFL++ (bucketizado) | Alta |
| **Context-sensitive** | Edge + call stack context | AFL++ CmpLog | Muy alta |

### Métodos de instrumentación

```
┌──────────────────────────────────────────────────────────────────────┐
│           Métodos de instrumentación                                 │
│                                                                      │
│  1. COMPILE-TIME (en tiempo de compilación)                          │
│     El compilador inserta los contadores.                            │
│     • afl-cc / afl-clang-fast (AFL++)                               │
│     • -fsanitize=fuzzer (libFuzzer/Clang)                           │
│     • -C instrument-coverage (Rust)                                  │
│     Ventaja: rápido, preciso                                        │
│     Requisito: tener el código fuente                                │
│                                                                      │
│  2. BINARY-ONLY (sin código fuente)                                  │
│     Se instrumenta el binario compilado.                             │
│     • QEMU mode (AFL++ -Q): emulación                               │
│     • Frida mode (AFL++ -O): instrumentación dinámica                │
│     • Intel PIN                                                      │
│     Ventaja: no necesita código fuente                               │
│     Desventaja: 2-10x más lento                                     │
│                                                                      │
│  3. HARDWARE-ASSISTED                                                │
│     Usa features del CPU para tracing.                               │
│     • Intel PT (Processor Trace)                                     │
│     • ARM ETM                                                        │
│     Ventaja: overhead casi cero                                      │
│     Desventaja: requiere hardware/kernel específico                  │
└──────────────────────────────────────────────────────────────────────┘
```

En este curso nos enfocamos en **compile-time instrumentation** porque:
- Es la más rápida
- Es la más precisa
- Tenemos el código fuente (estamos fuzzeando nuestro propio código)
- AFL++ y libFuzzer la usan por defecto

---

## 11. El corpus: la memoria del fuzzer

El corpus es la colección de inputs que el fuzzer ha descubierto como "interesantes"
(que cubren caminos únicos de código).

```
┌──────────────────────────────────────────────────────────────────────┐
│                   Anatomía del corpus                                │
│                                                                      │
│  corpus/                                                             │
│  ├── seed_001        (semilla original: archivo PNG válido)          │
│  ├── seed_002        (semilla original: otro PNG)                    │
│  ├── id:000000,orig:seed_001                                        │
│  │   (mutación de seed_001 que descubrió edge nuevo)                │
│  ├── id:000001,src:000000,op:flip1                                  │
│  │   (mutación de 000000 con bit flip, edge nuevo)                  │
│  ├── id:000002,src:000001,op:arith8                                 │
│  │   (aritmética sobre 000001, edge nuevo)                          │
│  ├── id:000003,src:000000,op:havoc                                  │
│  │   (havoc sobre 000000, edge nuevo)                               │
│  └── ...                                                             │
│                                                                      │
│  Cada archivo es el input más pequeño que activa un                  │
│  conjunto único de edges. El corpus CRECE con el tiempo.             │
│                                                                      │
│  Propiedades del corpus:                                             │
│  • Cada entrada cubre al menos un edge único                        │
│  • Es la "memoria" acumulada del fuzzer                             │
│  • Se puede reusar entre sesiones                                   │
│  • Se puede compartir entre equipos                                 │
│  • Se puede minimizar (reducir entradas redundantes)                │
└──────────────────────────────────────────────────────────────────────┘
```

### Ciclo de vida del corpus

```
  Sesión 1 (8 horas):
  corpus: 2 semillas → 150 entradas, 45% cobertura
  
  Sesión 2 (8 horas, reanudando con el corpus de sesión 1):
  corpus: 150 entradas → 280 entradas, 62% cobertura
  
  Sesión 3 (8 horas):
  corpus: 280 → 320 entradas, 68% cobertura
  
  ...la cobertura crece logarítmicamente.
  Cada sesión nueva empieza donde terminó la anterior.
```

---

## 12. Corpus inicial: semillas que aceleran la exploración

Un buen corpus inicial (semillas) puede reducir dramáticamente el tiempo que tarda
el fuzzer en llegar al código interesante.

### Estrategias para semillas

```
┌──────────────────────────────────────────────────────────────────────┐
│           Cómo elegir semillas para el corpus inicial                 │
│                                                                      │
│  1. ARCHIVOS VÁLIDOS PEQUEÑOS                                        │
│     Para un parser de PNG: incluir 3-5 PNGs de 1x1 o 2x2 píxeles   │
│     Para un parser de JSON: incluir JSONs mínimos                   │
│     Regla: lo más pequeño que sea válido                             │
│                                                                      │
│  2. ARCHIVOS QUE EJERCITAN FEATURES                                  │
│     Un PNG con transparencia (alpha channel)                         │
│     Un PNG interlazado                                               │
│     Un PNG con paleta de colores                                     │
│     Cada feature = caminos de código diferentes                      │
│                                                                      │
│  3. ARCHIVOS LIGERAMENTE INVÁLIDOS                                   │
│     Un PNG con CRC incorrecto                                        │
│     Un JSON con trailing comma                                       │
│     Para que el fuzzer explore los caminos de error                   │
│                                                                      │
│  4. ARCHIVOS VACÍOS Y MÍNIMOS                                       │
│     Un archivo vacío (0 bytes)                                       │
│     Un archivo de 1 byte                                             │
│     Sorprendentemente efectivos para encontrar edge cases            │
│                                                                      │
│  Lo que NO incluir:                                                  │
│  ✗ Archivos grandes (> 1KB para empezar)                            │
│  ✗ Archivos redundantes (muchas variantes del mismo tipo)            │
│  ✗ Archivos de producción reales (demasiado grandes/complejos)       │
└──────────────────────────────────────────────────────────────────────┘
```

### Ejemplo: semillas para un parser de CSV

```
corpus/
├── empty.csv        → ""
├── one_field.csv    → "hello"
├── one_row.csv      → "a,b,c"
├── two_rows.csv     → "a,b\n1,2"
├── quoted.csv       → '"hello, world",42'
├── escaped.csv      → '"he said ""hi""",ok'
├── unicode.csv      → "café,naïve,日本語"
├── numbers.csv      → "1,2.5,-3,0.0,1e10"
└── newlines.csv     → "a\r\nb\nc\r"
```

9 archivos pequeños que ejercitan: campos vacíos, quoting, escaping, unicode,
números en distintos formatos, y diferentes tipos de newline. El fuzzer puede
partir de cualquiera de estos y mutar para encontrar bugs.

---

## 13. Corpus minimization

Con el tiempo, el corpus acumula entradas redundantes (múltiples inputs que cubren
los mismos caminos). La minimización reduce el corpus al conjunto mínimo que
mantiene la misma cobertura.

```
┌──────────────────────────────────────────────────────────────────────┐
│              Corpus minimization                                     │
│                                                                      │
│  ANTES: 500 archivos en el corpus                                    │
│  Cobertura total: 2,340 edges                                       │
│  Tamaño total: 45 MB                                                │
│                                                                      │
│  DESPUÉS de minimización: 87 archivos                                │
│  Cobertura total: 2,340 edges (misma)                               │
│  Tamaño total: 2.1 MB                                               │
│                                                                      │
│  ¿Qué se eliminó?                                                    │
│  Entradas que no aportaban edges únicos.                             │
│  Si input_A cubre edges {1,2,3} e input_B cubre {1,2,3,4},          │
│  input_A es redundante (input_B ya cubre todo lo de A).              │
│                                                                      │
│  Beneficios:                                                         │
│  • Sesiones futuras arrancan más rápido                              │
│  • Menos disco                                                       │
│  • El fuzzer no pierde tiempo re-mutando inputs redundantes          │
└──────────────────────────────────────────────────────────────────────┘
```

### Herramientas de minimización

```bash
# AFL++: minimizar corpus
afl-cmin -i corpus_grande/ -o corpus_minimizado/ -- ./target @@

# Adicionalmente: minimizar cada archivo individual (hacerlo más pequeño)
afl-tmin -i crash_file -o crash_minimizado -- ./target @@

# libFuzzer: merge (es una minimización)
./fuzz_target -merge=1 corpus_minimizado/ corpus_grande/
```

La diferencia entre `cmin` y `tmin`:
- **cmin** (corpus minimize): reduce la **cantidad** de archivos en el corpus
- **tmin** (test case minimize): reduce el **tamaño** de un archivo individual

---

## 14. Crash discovery: qué busca el fuzzer

El fuzzer ejecuta el programa millones de veces. ¿Cómo sabe que encontró un bug?

```
┌──────────────────────────────────────────────────────────────────────┐
│            Señales de crash                                          │
│                                                                      │
│  El fuzzer intercepta señales del sistema operativo:                 │
│                                                                      │
│  SIGSEGV (11) — Segmentation Fault                                  │
│  ├── Acceso a memoria no mapeada                                    │
│  ├── Dereferencia de puntero nulo                                    │
│  ├── Buffer overflow que pasa el límite de la página                │
│  └── Stack overflow                                                  │
│                                                                      │
│  SIGABRT (6) — Abort                                                │
│  ├── assert() fallido                                               │
│  ├── free() de puntero inválido (double-free)                       │
│  ├── Sanitizer detectó violación                                    │
│  └── abort() explícito                                              │
│                                                                      │
│  SIGFPE (8) — Floating Point Exception                              │
│  ├── División por cero (enteros)                                    │
│  └── Overflow aritmético (en algunas arquitecturas)                  │
│                                                                      │
│  SIGBUS (7) — Bus Error                                             │
│  ├── Acceso a memoria con alineación incorrecta                     │
│  └── Acceso a región de memoria inválida                            │
│                                                                      │
│  TIMEOUT — No es una señal, el fuzzer lo detecta por tiempo         │
│  ├── Loop infinito                                                  │
│  ├── Complejidad algorítmica exponencial                            │
│  └── Deadlock                                                        │
│                                                                      │
│  Cuando cualquiera de estas ocurre, el fuzzer:                       │
│  1. Guarda el input que causó el crash                               │
│  2. Clasifica el crash (señal + ubicación)                           │
│  3. Continúa fuzzeando (busca más crashes)                           │
│  4. De-duplica crashes con el mismo stack trace                      │
└──────────────────────────────────────────────────────────────────────┘
```

### Clasificación de crashes

AFL++ clasifica los crashes por:

1. **Señal** que causó el crash
2. **Ubicación** en el código donde ocurrió (instruction pointer)
3. **Stack trace** (si hay sanitizers)

Dos crashes se consideran "el mismo bug" si tienen la misma señal y la misma
ubicación. Esto evita que el fuzzer reporte 10,000 variantes del mismo buffer
overflow.

---

## 15. Tipos de bugs que encuentra el fuzzing

### Bugs que el fuzzing encuentra bien

```
┌──────────────────────────────────────────────────────────────────────┐
│          Bugs que el fuzzing encuentra frecuentemente                 │
│                                                                      │
│  EN C:                                                               │
│  ├── Buffer overflows (stack y heap)                                │
│  ├── Use-after-free                                                  │
│  ├── Double-free                                                     │
│  ├── Null pointer dereference                                        │
│  ├── Integer overflow/underflow                                      │
│  ├── Off-by-one errors                                               │
│  ├── Format string vulnerabilities                                   │
│  ├── Uninitialized memory reads                                      │
│  ├── Stack exhaustion (recursión sin límite)                         │
│  └── Memory leaks (con LeakSanitizer)                                │
│                                                                      │
│  EN RUST (safe):                                                     │
│  ├── Panics (unwrap en None/Err)                                    │
│  ├── Index out of bounds (indexación directa)                        │
│  ├── Stack overflow (recursión)                                      │
│  ├── Arithmetic overflow (en debug mode)                             │
│  ├── Lógica incorrecta en parsers                                   │
│  └── Timeouts (complejidad algorítmica)                              │
│                                                                      │
│  EN RUST (unsafe):                                                   │
│  ├── Todo lo de C (buffer overflow, use-after-free, etc.)           │
│  ├── Data races                                                      │
│  └── Violaciones de aliasing                                         │
│                                                                      │
│  EN GO:                                                              │
│  ├── Panics (nil dereference, index out of range)                   │
│  ├── Deadlocks                                                       │
│  ├── Goroutine leaks                                                 │
│  └── Lógica incorrecta                                               │
└──────────────────────────────────────────────────────────────────────┘
```

### Bugs que el fuzzing NO encuentra bien

| Bug | Por qué no |
|---|---|
| Bugs lógicos de negocio | El fuzzer no sabe las reglas de negocio |
| Race conditions (sin TSan) | Dependen de timing, no de input |
| Bugs de rendimiento | El programa no crashea, solo es lento |
| Bugs de UI | No hay input/output de UI |
| Bugs de configuración | No dependen del input del usuario |
| Bugs de integración entre servicios | El fuzzer testea una función, no un sistema |

### Severidad por tipo

```
  Severidad
  CRÍTICA │  Buffer overflow (RCE)
          │  Use-after-free (RCE)
          │  Format string (RCE)
  ALTA    │  Integer overflow (corrupción de datos)
          │  Null deref (DoS)
          │  Double-free (corrupción de heap)
  MEDIA   │  Uninitialized read (info leak)
          │  Memory leak (DoS lento)
          │  Stack overflow (DoS)
  BAJA    │  Panic en Rust safe (DoS controlado)
          │  Timeout (DoS)
          └──────────────────────────────────────

  RCE = Remote Code Execution (el atacante ejecuta código arbitrario)
  DoS = Denial of Service (el programa deja de funcionar)
```

---

## 16. El harness: la función target

El harness (o target function) es la función que el fuzzer llama con cada input
generado. Es el "pegamento" entre el fuzzer y el código que quieres testear.

### Anatomía de un harness en C (para libFuzzer)

```c
/* harness.c — el fuzzer llamará a esta función millones de veces */
#include <stdint.h>
#include <stddef.h>

/* Función que queremos fuzzear */
extern int parse_data(const char *data, size_t len);

/* Función que libFuzzer busca: punto de entrada del harness */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Llamar a la función target con los bytes generados por el fuzzer */
    parse_data((const char *)data, size);

    /* Retornar 0 siempre (no hay "fallo" del harness) */
    /* Los crashes se detectan por señales (SIGSEGV, SIGABRT, etc.) */
    return 0;
}
```

### Anatomía de un harness en C (para AFL++)

```c
/* harness_afl.c — AFL lee de stdin o de un archivo */
#include <stdio.h>
#include <stdlib.h>

extern int parse_data(const char *data, size_t len);

int main(int argc, char *argv[]) {
    /* Leer todo el input desde stdin o archivo */
    FILE *f;
    if (argc > 1) {
        f = fopen(argv[1], "rb");
    } else {
        f = stdin;
    }

    /* Leer hasta EOF */
    char buf[65536];
    size_t len = fread(buf, 1, sizeof(buf), f);

    if (argc > 1) fclose(f);

    /* Llamar a la función target */
    parse_data(buf, len);

    return 0;
}
```

### Reglas para un buen harness

```
┌──────────────────────────────────────────────────────────────────────┐
│             Reglas de un buen harness                                 │
│                                                                      │
│  1. SIN SIDE EFFECTS PERSISTENTES                                    │
│     ✗ No escribir archivos                                           │
│     ✗ No modificar estado global entre llamadas                      │
│     ✗ No abrir conexiones de red                                     │
│     ✓ Cada llamada debe ser independiente                            │
│                                                                      │
│  2. RÁPIDO                                                           │
│     El harness se ejecuta millones de veces.                         │
│     Cada microsegundo cuenta.                                        │
│     ✗ No inicializar recursos pesados en cada llamada                │
│     ✓ Inicializar una vez (en main o con __attribute__((constructor)))│
│                                                                      │
│  3. DETERMINISTA                                                     │
│     Mismo input → mismo comportamiento.                              │
│     ✗ No usar rand(), time(), getpid()                               │
│     ✗ No depender de threads o scheduling                            │
│     ✓ El resultado solo depende del input                            │
│                                                                      │
│  4. SIN SALIDA A TERMINAL                                            │
│     ✗ No printf/fprintf a stdout/stderr                              │
│     (abruma al fuzzer y reduce velocidad)                            │
│     ✓ Solo retornar 0                                                │
│                                                                      │
│  5. MANEJAR INPUTS VACÍOS Y GRANDES                                 │
│     ✓ if (size == 0) return 0;  (no crashear con input vacío)       │
│     ✓ if (size > MAX) return 0; (limitar inputs absurdamente grandes)│
│                                                                      │
│  6. LIBERAR MEMORIA                                                  │
│     Aunque el proceso termine, liberar memoria ayuda a:              │
│     • Detectar memory leaks con LeakSanitizer                        │
│     • Evitar OOM en modo persistente (libFuzzer)                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Harness con inicialización una sola vez

```c
/* Para funciones que necesitan setup costoso */
#include <stdint.h>
#include <stddef.h>

static ParserContext *ctx = NULL;

/* Se llama una sola vez antes de empezar el fuzzing */
int LLVMFuzzerInitialize(int *argc, char ***argv) {
    ctx = parser_context_create();  /* setup costoso: una vez */
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    parser_context_reset(ctx);      /* reset barato: cada iteración */
    parser_parse(ctx, data, size);
    return 0;
}
```

---

## 17. Anatomía de una sesión de fuzzing

### Flujo completo con AFL++

```
┌──────────────────────────────────────────────────────────────────────┐
│           Sesión de fuzzing completa con AFL++                        │
│                                                                      │
│  PREPARACIÓN                                                         │
│  ───────────                                                         │
│  1. Compilar el target con instrumentación:                          │
│     $ afl-cc -o target_fuzz target.c harness.c                      │
│                                                                      │
│  2. Preparar corpus inicial:                                         │
│     $ mkdir corpus_in                                                │
│     $ echo "hello" > corpus_in/seed1                                │
│     $ echo '{"key": 1}' > corpus_in/seed2                           │
│                                                                      │
│  3. Crear directorio de output:                                      │
│     $ mkdir corpus_out                                               │
│                                                                      │
│  EJECUCIÓN                                                           │
│  ─────────                                                           │
│  4. Lanzar AFL++:                                                    │
│     $ afl-fuzz -i corpus_in -o corpus_out -- ./target_fuzz @@       │
│                                                                      │
│     -i: directorio con semillas iniciales                            │
│     -o: directorio donde AFL guarda resultados                       │
│     @@: AFL reemplaza esto con el nombre del archivo de input        │
│                                                                      │
│  MONITOREO                                                           │
│  ─────────                                                           │
│  5. AFL muestra un dashboard en tiempo real:                         │
│                                                                      │
│     ┌─ process timing ──────────────────────────────────┐            │
│     │        run time : 0 days, 3 hrs, 42 min, 17 sec  │            │
│     │   last new find : 0 days, 0 hrs, 2 min, 45 sec   │            │
│     │  last uniq crash : 0 days, 1 hrs, 12 min, 5 sec  │            │
│     ├─ overall results ─────────────────────────────────┤            │
│     │  cycles done : 12                                 │            │
│     │  total paths : 1,847                              │            │
│     │ uniq crashes : 3                                  │            │
│     │   uniq hangs : 1                                  │            │
│     ├─ cycle progress ──────────────────────────────────┤            │
│     │  now processing : 1423 (77.04%)                   │            │
│     ├─ map coverage ────────────────────────────────────┤            │
│     │    map density : 4.72% / 8.35%                    │            │
│     ├─ stage progress ──────────────────────────────────┤            │
│     │  now trying : havoc                               │            │
│     │ stage execs : 34.2k/131k (26.11%)                 │            │
│     │ total execs : 18.4M                               │            │
│     │  exec speed : 1,842/sec                           │            │
│     └───────────────────────────────────────────────────┘            │
│                                                                      │
│  RESULTADOS                                                          │
│  ──────────                                                          │
│  6. Los resultados están en corpus_out/:                             │
│                                                                      │
│     corpus_out/                                                      │
│     ├── default/                                                     │
│     │   ├── queue/         ← corpus: inputs interesantes            │
│     │   │   ├── id:000000,...                                        │
│     │   │   ├── id:000001,...                                        │
│     │   │   └── ...                                                  │
│     │   ├── crashes/       ← inputs que causan crashes              │
│     │   │   ├── id:000000,...                                        │
│     │   │   └── README.txt                                           │
│     │   ├── hangs/         ← inputs que causan timeouts             │
│     │   │   └── id:000000,...                                        │
│     │   ├── fuzzer_stats   ← estadísticas                           │
│     │   └── plot_data      ← datos para graficar                    │
│     └── ...                                                          │
│                                                                      │
│  POST-PROCESAMIENTO                                                  │
│  ──────────────────                                                  │
│  7. Reproducir un crash:                                             │
│     $ ./target_fuzz corpus_out/default/crashes/id:000000,...         │
│     Segmentation fault                                               │
│                                                                      │
│  8. Minimizar el crash:                                              │
│     $ afl-tmin -i crash_file -o crash_min -- ./target_fuzz @@       │
│                                                                      │
│  9. Analizar con sanitizer:                                          │
│     $ afl-cc -fsanitize=address -o target_asan target.c harness.c   │
│     $ ./target_asan crash_min                                        │
│     (ASan da stack trace y tipo exacto de bug)                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 18. Métricas de una sesión de fuzzing

### Métricas clave del dashboard de AFL++

| Métrica | Qué mide | Valores buenos |
|---|---|---|
| **exec speed** | Ejecuciones del target por segundo | > 500/sec para C, > 100/sec para programas complejos |
| **total paths** | Inputs en el corpus (caminos únicos descubiertos) | Creciendo logarítmicamente |
| **map density** | % del mapa de cobertura cubierto (curr% / total%) | > 5% es razonable |
| **uniq crashes** | Crashes de-duplicados encontrados | Cualquier número > 0 es un resultado |
| **uniq hangs** | Timeouts de-duplicados encontrados | Investigar si > 0 |
| **cycles done** | Veces que AFL recorrió todo el corpus | >= 1 es mínimo aceptable |
| **last new find** | Tiempo desde el último input interesante | Si > 1 hora, considerar parar |
| **stability** | % de determinismo de la cobertura | > 90% ideal, < 80% investigar |

### Cuándo parar de fuzzear

```
┌──────────────────────────────────────────────────────────────────────┐
│           Señales de que puedes parar                                 │
│                                                                      │
│  1. "last new find" lleva horas sin actividad                        │
│     → El fuzzer no está encontrando caminos nuevos                   │
│     → Los retornos marginales son mínimos                            │
│                                                                      │
│  2. "cycles done" >= 3                                               │
│     → AFL recorrió el corpus completo al menos 3 veces              │
│     → Ya aplicó todas las estrategias de mutación                    │
│                                                                      │
│  3. La curva de cobertura se aplanó                                  │
│     → Los primeros minutos descubren mucho                           │
│     → Después de horas, cada nuevo path es raro                      │
│                                                                      │
│  4. No tienes más tiempo/recursos                                    │
│     → Fuzzing es una actividad de "cuanto más, mejor"                │
│     → Pero 5 minutos de fuzzing es mejor que cero                    │
│                                                                      │
│  Guía de tiempo por tipo de target:                                  │
│                                                                      │
│  • Función simple (< 100 LOC): 5-30 minutos                         │
│  • Parser mediano (100-1000 LOC): 1-4 horas                         │
│  • Parser complejo (> 1000 LOC): 8-24 horas                         │
│  • Librería de producción: días-semanas (CI continuo)                │
│  • Fuzzing de seguridad: semanas-meses (OSS-Fuzz)                    │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 19. Diccionarios: ayudar al fuzzer con tokens

Un diccionario es una lista de tokens (strings o bytes) que son significativos para
el formato que estás fuzzeando. El fuzzer usa estos tokens en sus mutaciones para
generar inputs más inteligentes.

### Ejemplo: diccionario para JSON

```
# json.dict — tokens relevantes para un parser JSON

# Delimitadores
"{"
"}"
"["
"]"
":"
","

# Valores especiales
"true"
"false"
"null"

# Strings
"\""
"\\"
"\\n"
"\\t"
"\\u0000"
"\\uFFFF"

# Números
"0"
"-1"
"1e10"
"1e-10"
"1.0"
"-0"
"0.0"
"1E+100"
"9999999999999999999"

# Whitespace
" "
"\t"
"\n"
"\r"
```

### Cómo los usa el fuzzer

Sin diccionario, el fuzzer tendría que "descubrir" por mutación aleatoria que `true`
es un token significativo — necesitaría alinear los bytes `t`, `r`, `u`, `e` en la
posición correcta. Con diccionario, el fuzzer puede insertar `true` directamente.

```
  Sin diccionario:
  Mutación aleatoria intenta: "xrue" → "trxe" → "true" (eventual, lento)
  
  Con diccionario:
  El fuzzer inserta "true" directamente en posiciones aleatorias.
  Resultado: llega al código de parseo de booleanos MUCHO más rápido.
```

### Uso con AFL++

```bash
# Usar un diccionario
afl-fuzz -i corpus/ -o output/ -x json.dict -- ./parser_fuzz @@
```

### Uso con libFuzzer

```bash
# El diccionario se pasa como argumento
./fuzz_target -dict=json.dict corpus/
```

### Diccionarios predefinidos

AFL++ y libFuzzer incluyen diccionarios para formatos comunes:

```
AFL++/dictionaries/
├── gif.dict
├── jpeg.dict
├── json.dict
├── png.dict
├── pdf.dict
├── html.dict
├── http.dict
├── sql.dict
├── xml.dict
├── tiff.dict
└── ...
```

### Generar diccionarios automáticamente

```bash
# AFL++ puede extraer tokens del binario automáticamente
# (busca strings en el código compilado)
afl-fuzz -i corpus/ -o output/ -x @auto -- ./parser_fuzz @@
```

El modo `@auto` extrae tokens de comparaciones (strings usadas en `strcmp`,
`memcmp`, etc.) encontradas durante la instrumentación.

---

## 20. Sanitizers: amplificar la detección

Los sanitizers son herramientas del compilador que detectan bugs en runtime que
normalmente serían silenciosos (no causan crash inmediato).

```
┌──────────────────────────────────────────────────────────────────────┐
│          Fuzzing + Sanitizers = detección amplificada                 │
│                                                                      │
│  Sin sanitizer:                                                      │
│  char buf[10];                                                       │
│  buf[10] = 'x';  // buffer overflow — podría NO crashear            │
│                   // (escribe en memoria adyacente sin protección)    │
│                   // El fuzzer NO detecta este bug                    │
│                                                                      │
│  Con AddressSanitizer (ASan):                                        │
│  char buf[10];                                                       │
│  buf[10] = 'x';  // ASan detecta inmediatamente:                    │
│                   // ==12345==ERROR: AddressSanitizer:                │
│                   // heap-buffer-overflow on address 0x...            │
│                   // WRITE of size 1 at 0x... thread T0              │
│                   // → SIGABRT → el fuzzer lo captura                │
│                                                                      │
│  Sanitizers convierten bugs SILENCIOSOS en crashes DETECTABLES.      │
│  Esto multiplica la efectividad del fuzzer dramáticamente.           │
└──────────────────────────────────────────────────────────────────────┘
```

### Los sanitizers principales

| Sanitizer | Abreviatura | Detecta | Flag de compilación |
|---|---|---|---|
| AddressSanitizer | ASan | Buffer overflow, use-after-free, double-free, stack overflow | `-fsanitize=address` |
| MemorySanitizer | MSan | Lectura de memoria no inicializada | `-fsanitize=memory` |
| UndefinedBehaviorSanitizer | UBSan | Signed overflow, null deref, shift overflow | `-fsanitize=undefined` |
| ThreadSanitizer | TSan | Data races, deadlocks | `-fsanitize=thread` |
| LeakSanitizer | LSan | Memory leaks | `-fsanitize=leak` (o con ASan) |

### Compilar para fuzzing con sanitizers

```bash
# AFL++ con ASan
afl-cc -fsanitize=address,undefined -o target_fuzz target.c harness.c

# libFuzzer con ASan (libFuzzer ya incluye -fsanitize=fuzzer)
clang -fsanitize=fuzzer,address,undefined -o fuzz_target target.c

# Nota: ASan y MSan NO se pueden usar juntos.
# Elige uno u otro.
```

### Impacto en rendimiento

| Sanitizer | Slowdown | Memory overhead |
|---|---|---|
| ASan | 2x | 2-3x |
| MSan | 3x | 2x |
| UBSan | < 1.5x | Mínimo |
| TSan | 5-15x | 5-10x |

Aunque los sanitizers hacen el programa más lento, el beneficio en detección de bugs
compensa con creces. La recomendación estándar es **siempre fuzzear con ASan+UBSan**.

---

## 21. Fuzzing determinista vs no determinista

### Determinismo en el contexto de fuzzing

```
┌──────────────────────────────────────────────────────────────────────┐
│         Determinismo en fuzzing                                      │
│                                                                      │
│  DETERMINISTA: mismo input → misma cobertura siempre                │
│  ────────────────────────────────────────────────                    │
│  La función f(x) siempre ejecuta los mismos caminos para x.         │
│  Ideal para fuzzing: el fuzzer puede razonar sobre el mapa           │
│  de cobertura.                                                       │
│                                                                      │
│  NO DETERMINISTA: mismo input → cobertura variable                   │
│  ────────────────────────────────────────────────                    │
│  La función f(x) ejecuta caminos diferentes cada vez                 │
│  (por threads, timers, rand(), etc.)                                 │
│  Problema: el fuzzer no puede distinguir "nuevo camino"              │
│  de "misma ejecución con timing diferente".                          │
│                                                                      │
│  AFL++ muestra esto como "stability":                                │
│  stability > 95%  → determinista, fuzzing efectivo                   │
│  stability 80-95% → algo de no-determinismo, aceptable              │
│  stability < 80%  → problema: el fuzzer pierde eficiencia            │
│                                                                      │
│  Fuentes comunes de no-determinismo:                                 │
│  • rand() / random()                                                 │
│  • time() / gettimeofday()                                           │
│  • getpid() / pthread IDs                                            │
│  • ASLR (Address Space Layout Randomization)                         │
│  • Threads con scheduling no determinista                            │
│  • mmap sin MAP_FIXED                                                │
│  • Hash maps con randomized hashing                                  │
│                                                                      │
│  Soluciones:                                                         │
│  • Mockear rand/time en el harness                                   │
│  • Fijar la semilla de RNG                                           │
│  • Deshabilitar ASLR: echo 0 > /proc/sys/kernel/randomize_va_space  │
│  • Usar single-threaded si es posible                                │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 22. Reproducibilidad: del crash al test case

Cuando el fuzzer encuentra un crash, el trabajo no termina — empieza el análisis.

### Pipeline de triaje

```
┌──────────────────────────────────────────────────────────────────────┐
│           Pipeline: crash → bug fix → regression test                │
│                                                                      │
│  1. REPRODUCIR                                                       │
│     $ ./target crashes/id:000000,...                                 │
│     Segmentation fault (core dumped)                                 │
│     Confirmar que el crash es reproducible.                          │
│                                                                      │
│  2. MINIMIZAR                                                        │
│     $ afl-tmin -i crashes/id:000000 -o crash_min -- ./target @@     │
│     El input pasa de 4096 bytes a 23 bytes.                          │
│     Input mínimo que reproduce el crash.                             │
│                                                                      │
│  3. ANALIZAR CON SANITIZER                                           │
│     $ gcc -fsanitize=address -g -o target_asan target.c             │
│     $ ./target_asan crash_min                                        │
│     ==PID==ERROR: AddressSanitizer: heap-buffer-overflow             │
│       READ of size 1 at 0x...                                        │
│       #0 parse_field() at target.c:47                                │
│       #1 parse_record() at target.c:82                               │
│       #2 main() at harness.c:15                                      │
│     → Sabemos: overflow en parse_field, línea 47                     │
│                                                                      │
│  4. ANALIZAR CON DEBUGGER                                            │
│     $ gdb ./target_asan                                              │
│     (gdb) run crash_min                                              │
│     (gdb) bt          ← backtrace completo                          │
│     (gdb) frame 0     ← ir al frame del crash                       │
│     (gdb) print len   ← inspeccionar variables                      │
│     → Entender la causa raíz                                         │
│                                                                      │
│  5. CORREGIR EL BUG                                                  │
│     Editar target.c línea 47: agregar bounds check                   │
│                                                                      │
│  6. VERIFICAR EL FIX                                                 │
│     $ gcc -fsanitize=address -o target_fixed target.c               │
│     $ ./target_fixed crash_min                                       │
│     (sin crash — fix confirmado)                                     │
│                                                                      │
│  7. AGREGAR REGRESSION TEST                                          │
│     Convertir crash_min en un test case permanente:                  │
│                                                                      │
│     void test_crash_000000(void) {                                   │
│         const uint8_t input[] = { ... bytes de crash_min ... };      │
│         int result = parse_data(input, sizeof(input));               │
│         /* Ahora debería retornar error, no crashear */              │
│         assert(result == PARSE_ERROR);                               │
│     }                                                                │
│                                                                      │
│  8. AGREGAR AL CORPUS                                                │
│     $ cp crash_min corpus/regression_000000                          │
│     El fuzzer usará este input como semilla en el futuro.            │
│     Si alguien reintroduce el bug, el fuzzer lo encontrará           │
│     rápidamente.                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 23. Cuándo usar fuzzing (y cuándo no)

### Excelente para

```
┌──────────────────────────────────────────────────────────────────────┐
│          Cuándo el fuzzing brilla                                    │
│                                                                      │
│  PARSERS Y DESERIALIZADORES                                          │
│  ├── JSON, XML, YAML, TOML, CSV parsers                            │
│  ├── Decodificadores de imagen (PNG, JPEG, GIF, WebP)               │
│  ├── Decodificadores de audio/video (MP3, H.264)                    │
│  ├── Protocolos de red (HTTP, DNS, TLS, MQTT)                       │
│  ├── Formatos de archivo (ZIP, PDF, ELF, PE)                        │
│  └── Compiladores y lenguajes (parsers de código fuente)             │
│                                                                      │
│  CÓDIGO QUE PROCESA INPUT NO CONFIABLE                               │
│  ├── Web servers (HTTP requests)                                     │
│  ├── Servidores de base de datos (SQL queries)                       │
│  ├── APIs públicas (cualquier request body)                          │
│  └── Herramientas CLI que leen archivos del usuario                  │
│                                                                      │
│  CÓDIGO CON UNSAFE (C, C++, Rust unsafe)                             │
│  ├── Buffers manuales y aritmética de punteros                      │
│  ├── FFI bindings                                                    │
│  └── Algoritmos con manejo manual de memoria                         │
│                                                                      │
│  CODECS Y TRANSFORMACIONES                                           │
│  ├── Compresión/descompresión (zlib, lz4, zstd)                     │
│  ├── Encriptación/desencriptación                                   │
│  └── Encoding/decoding (base64, UTF-8, URL encoding)                │
│                                                                      │
│  PROPIEDAD CLAVE: el target acepta bytes y los procesa.              │
│  Si puedes escribir: f(bytes) → resultado, puedes fuzzearlo.         │
└──────────────────────────────────────────────────────────────────────┘
```

### No recomendado para

| Escenario | Por qué no | Alternativa |
|---|---|---|
| Lógica de negocio pura | Los bugs son lógicos, no crashes | Property-based testing |
| UI/Frontend | El input no es bytes crudos | Selenium, Cypress |
| Integración entre servicios | Necesitas el sistema completo | Integration tests |
| Configuración | Pocos inputs posibles | Unit tests |
| Código sin side effects (funciones puras simples) | Difícil que crasheen | Property-based testing |
| Código que solo llama a librerías maduras | Los bugs están en la librería, no en tu código | Testear la librería directamente |

---

## 24. El ecosistema de fuzzers

```
┌──────────────────────────────────────────────────────────────────────┐
│              Ecosistema de fuzzers principales                       │
│                                                                      │
│  COVERAGE-GUIDED (los más usados hoy)                                │
│  ─────────────────────────────────────                               │
│  AFL++ (American Fuzzy Lop Plus Plus)                                │
│  │  Lenguajes: C, C++ (y cualquiera via QEMU/Frida)                │
│  │  Modo: proceso externo (fork/exec o persistente)                 │
│  │  Fuerte: versatilidad, modos custom, CmpLog                     │
│  │  Ideal para: fuzzing de binarios, proyectos C/C++               │
│  │                                                                   │
│  libFuzzer (LLVM)                                                    │
│  │  Lenguajes: C, C++, Rust (via cargo-fuzz)                       │
│  │  Modo: in-process (función linkada)                              │
│  │  Fuerte: velocidad (sin fork), integración con sanitizers        │
│  │  Ideal para: librerías, funciones individuales                   │
│  │                                                                   │
│  Honggfuzz (Google)                                                  │
│  │  Lenguajes: C, C++ (+ binary via Intel PT)                      │
│  │  Modo: proceso externo con hardware tracing                      │
│  │  Fuerte: feedback de hardware (Intel PT), parallel fuzzing        │
│  │  Ideal para: kernels, drivers, código de alta performance        │
│  │                                                                   │
│  cargo-fuzz (Rust)                                                   │
│  │  Lenguajes: Rust                                                 │
│  │  Modo: libFuzzer backend                                         │
│  │  Fuerte: integración nativa con Cargo, Arbitrary trait           │
│  │  Ideal para: crates Rust, especialmente parsers                  │
│  │                                                                   │
│  go-fuzz / go test -fuzz (Go)                                        │
│     Lenguajes: Go                                                    │
│     Modo: integrado en go test (Go 1.18+)                           │
│     Fuerte: cero configuración, parte del toolchain                 │
│     Ideal para: cualquier código Go                                  │
│                                                                      │
│  GENERATION-BASED                                                    │
│  ─────────────────                                                   │
│  Peach / Peach Community                                             │
│  │  Formato modelable con XML                                       │
│  │  Ideal para: protocolos de red, formatos complejos               │
│  │                                                                   │
│  Domato (Google)                                                     │
│     Generador de gramáticas para HTML/CSS/JS                         │
│     Ideal para: fuzzing de browsers                                  │
│                                                                      │
│  ESPECIALIZADOS                                                      │
│  ──────────────                                                      │
│  syzkaller (Google)                                                  │
│  │  Fuzzing de system calls del kernel Linux                        │
│  │  Ha encontrado miles de bugs en el kernel                        │
│  │                                                                   │
│  OSS-Fuzz (Google)                                                   │
│     Infraestructura de fuzzing continuo para open source             │
│     ClusterFuzz: distributed fuzzing a escala                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 25. Fuzzing en C vs Rust vs Go

| Aspecto | C | Rust | Go |
|---|---|---|---|
| **Fuzzer principal** | AFL++, libFuzzer | cargo-fuzz (libFuzzer) | go test -fuzz |
| **Integración** | Manual (compilar harness) | Cargo nativo | go test nativo |
| **Setup** | Instalar AFL++, compilar con afl-cc | `cargo install cargo-fuzz`, `cargo fuzz init` | Nada (built-in desde Go 1.18) |
| **Harness** | `LLVMFuzzerTestOneInput` o `main` con stdin | `fuzz_target!` macro | `func FuzzXxx(f *testing.F)` |
| **Input type** | `uint8_t *data, size_t size` | `&[u8]` o tipos `Arbitrary` | `[]byte`, `string`, `int`, etc. |
| **Inputs estructurados** | Manual (parsear bytes) | `Arbitrary` trait | Nativo (tipos Go directos) |
| **Sanitizers** | ASan, MSan, UBSan, TSan | ASan (nightly), Miri | Race detector (`-race`) |
| **Bugs encontrados** | Memory corruption, UB, crashes | Panics, logic errors, overflow | Panics, logic errors |
| **Severidad típica** | Alta (RCE, privilege escalation) | Media (DoS por panic) | Media (DoS por panic) |
| **Velocidad** | Muy rápida (> 1000/sec) | Rápida (> 500/sec) | Media (> 100/sec) |
| **CI integración** | OSS-Fuzz, manual | cargo-fuzz + CI, OSS-Fuzz | go test -fuzz nativo |

### Ejemplo mínimo en cada lenguaje

**C (libFuzzer)**:
```c
#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    parse(data, size);
    return 0;
}
```

Compilar:
```bash
clang -fsanitize=fuzzer,address -o fuzz fuzz.c parser.c
./fuzz corpus/
```

**Rust (cargo-fuzz)**:
```rust
// fuzz/fuzz_targets/fuzz_parser.rs
#![no_main]
use libfuzzer_sys::fuzz_target;

fuzz_target!(|data: &[u8]| {
    let _ = my_crate::parse(data);
});
```

Ejecutar:
```bash
cargo fuzz run fuzz_parser
```

**Go (go test -fuzz)**:
```go
func FuzzParse(f *testing.F) {
    f.Add([]byte("hello"))  // semilla
    f.Fuzz(func(t *testing.T, data []byte) {
        parse(data)  // si panics, go test lo reporta
    })
}
```

Ejecutar:
```bash
go test -fuzz=FuzzParse -fuzztime=60s
```

---

## 26. Errores comunes al empezar con fuzzing

### Error 1: no usar sanitizers

```
❌ Problema:
  Fuzzeas durante 8 horas sin sanitizers.
  Encuentras 0 crashes.
  Conclusión: "el código está limpio".
  Realidad: hay 15 buffer overflows que no crashean sin ASan.

✓ Solución:
  SIEMPRE compilar con -fsanitize=address,undefined:
  afl-cc -fsanitize=address,undefined -o target target.c
  
  O con libFuzzer:
  clang -fsanitize=fuzzer,address,undefined -o fuzz fuzz.c
```

### Error 2: corpus inicial vacío

```
❌ Problema:
  No proporcionas semillas. El fuzzer empieza con bytes aleatorios.
  Para un parser de XML, pasa horas intentando generar "<" por mutación.

✓ Solución:
  Siempre proporcionar al menos 3-5 semillas pequeñas:
  echo '<root/>' > corpus/seed1.xml
  echo '<a><b>text</b></a>' > corpus/seed2.xml
  echo '<?xml version="1.0"?><x/>' > corpus/seed3.xml
```

### Error 3: harness demasiado lento

```
❌ Problema:
  El harness inicializa una base de datos en cada iteración.
  exec speed: 3/sec. En 8 horas: solo 86,400 ejecuciones.
  Para contexto: AFL++ en un target eficiente hace 86,400 en 1 minuto.

✓ Solución:
  Usar LLVMFuzzerInitialize para setup costoso.
  El harness debe ser RÁPIDO: solo llamar a la función target.
  Target de velocidad: > 500 exec/sec para C.
```

### Error 4: fuzzear demasiado poco tiempo

```
❌ Problema:
  Ejecutas el fuzzer por 30 segundos, no encuentra crashes.
  Conclusión: "no hay bugs".

✓ Solución:
  Mínimo recomendado:
  • Función simple: 5 minutos
  • Parser: 1 hora
  • Librería completa: 8+ horas
  • Código de seguridad: días (CI continuo)
  
  Regla: si "cycles done" < 1, no has fuzzeado suficiente.
```

### Error 5: ignorar los hangs

```
❌ Problema:
  El fuzzer reporta 5 hangs pero te enfocas solo en los crashes.
  Los hangs son timeouts que podrían ser:
  • Loops infinitos (bug real)
  • Complejidad algorítmica O(n²) o peor (ReDoS, hash collision)
  • Deadlocks

✓ Solución:
  Investigar cada hang:
  $ timeout 5 ./target hangs/id:000000,...
  Si cuelga consistentemente, es un bug que causa DoS.
```

### Error 6: no minimizar los crashes

```
❌ Problema:
  El crash file tiene 50KB. ¿Cuáles de esos bytes son relevantes?
  Debugging es una pesadilla.

✓ Solución:
  Siempre minimizar antes de analizar:
  $ afl-tmin -i crash -o crash_min -- ./target @@
  
  El crash_min podría ser solo 10 bytes — mucho más fácil de entender.
```

### Error 7: fuzzear el programa completo en lugar de funciones

```
❌ Problema:
  Fuzzeas el binario completo (./mi_servidor).
  El fuzzer pierde tiempo en parsing de CLI, init de logging,
  conexiones de red, etc.
  exec speed: 10/sec.

✓ Solución:
  Escribir un harness que llame DIRECTAMENTE a la función
  que procesa input:
  
  // En vez de fuzzear todo el servidor HTTP:
  int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
      // Fuzzear solo el parser de HTTP requests
      parse_http_request(data, size);
      return 0;
  }
  
  exec speed: 5000/sec. 500x más rápido.
```

### Error 8: no convertir crashes en regression tests

```
❌ Problema:
  Encuentras 3 crashes, los arreglas, y borras los crash files.
  Semanas después, alguien reintroduce el mismo bug.
  Nadie se entera hasta producción.

✓ Solución:
  Cada crash → un test case permanente:
  1. Minimizar el crash
  2. Convertirlo en un unit test
  3. Agregarlo al corpus de semillas
  4. Commitearlo al repositorio
```

---

## 27. Ejemplo completo: fuzzear un parser de CSV en C

### El código del parser (con bugs intencionales)

```c
/* csv_parser.h */
#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <stddef.h>

#define CSV_MAX_FIELDS  64
#define CSV_MAX_FIELD_LEN 256

typedef struct {
    char fields[CSV_MAX_FIELDS][CSV_MAX_FIELD_LEN];
    int field_count;
} CsvRow;

typedef struct {
    CsvRow *rows;
    int row_count;
    int capacity;
} CsvTable;

/* Parsea un buffer CSV y retorna una tabla.
 * El llamador debe liberar con csv_table_free(). */
CsvTable *csv_parse(const char *data, size_t len);
void csv_table_free(CsvTable *table);

#endif
```

```c
/* csv_parser.c — parser con bugs intencionales para que el fuzzer encuentre */
#include "csv_parser.h"
#include <stdlib.h>
#include <string.h>

CsvTable *csv_parse(const char *data, size_t len) {
    CsvTable *table = malloc(sizeof(CsvTable));
    if (!table) return NULL;

    table->capacity = 16;
    table->row_count = 0;
    table->rows = malloc(sizeof(CsvRow) * table->capacity);
    if (!table->rows) {
        free(table);
        return NULL;
    }

    int current_field = 0;
    int field_pos = 0;
    int in_quotes = 0;

    /* Inicializar primera fila */
    memset(&table->rows[0], 0, sizeof(CsvRow));

    for (size_t i = 0; i < len; i++) {
        char c = data[i];

        if (c == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        if (in_quotes) {
            /* BUG 1: no verifica field_pos < CSV_MAX_FIELD_LEN */
            /* Un campo quoted muy largo causa buffer overflow */
            table->rows[table->row_count].fields[current_field][field_pos] = c;
            field_pos++;
            continue;
        }

        if (c == ',') {
            /* Terminar campo actual */
            table->rows[table->row_count].fields[current_field][field_pos] = '\0';
            current_field++;
            field_pos = 0;

            /* BUG 2: no verifica current_field < CSV_MAX_FIELDS */
            /* Demasiados campos causa buffer overflow */
            continue;
        }

        if (c == '\n') {
            /* Terminar fila */
            table->rows[table->row_count].fields[current_field][field_pos] = '\0';
            table->rows[table->row_count].field_count = current_field + 1;
            table->row_count++;

            /* BUG 3: no verifica row_count < capacity antes de acceder rows[] */
            /* Si no se hace realloc, se escribe fuera del buffer */
            if (table->row_count >= table->capacity) {
                table->capacity *= 2;
                table->rows = realloc(table->rows,
                    sizeof(CsvRow) * table->capacity);
                /* BUG 4: no verifica si realloc retorna NULL */
            }

            current_field = 0;
            field_pos = 0;
            memset(&table->rows[table->row_count], 0, sizeof(CsvRow));
            continue;
        }

        /* Carácter normal */
        if (field_pos < CSV_MAX_FIELD_LEN - 1) {
            table->rows[table->row_count].fields[current_field][field_pos] = c;
            field_pos++;
        }
        /* Nota: fuera de quotes sí tiene bounds check (field_pos).
           Pero dentro de quotes (BUG 1) no. */
    }

    /* Última fila (sin newline final) */
    if (field_pos > 0 || current_field > 0) {
        table->rows[table->row_count].fields[current_field][field_pos] = '\0';
        table->rows[table->row_count].field_count = current_field + 1;
        table->row_count++;
    }

    return table;
}

void csv_table_free(CsvTable *table) {
    if (table) {
        free(table->rows);
        free(table);
    }
}
```

### El harness

```c
/* fuzz_csv.c — harness para libFuzzer */
#include <stdint.h>
#include <stddef.h>
#include "csv_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Limitar tamaño para evitar timeouts */
    if (size > 65536) return 0;

    /* Parsear y liberar */
    CsvTable *table = csv_parse((const char *)data, size);
    if (table) {
        csv_table_free(table);
    }
    return 0;
}
```

### Compilar y ejecutar

```bash
# Compilar con libFuzzer + ASan
clang -fsanitize=fuzzer,address -g -o fuzz_csv fuzz_csv.c csv_parser.c

# Crear corpus inicial
mkdir corpus
echo 'a,b,c' > corpus/simple.csv
echo '"hello","world"' > corpus/quoted.csv
echo 'a,b
1,2
3,4' > corpus/multirow.csv

# Crear diccionario
cat > csv.dict << 'EOF'
","
"\n"
"\r\n"
"\""
"\"\""
","
""
" "
EOF

# Ejecutar fuzzer
./fuzz_csv -dict=csv.dict corpus/
```

### Qué encontrará el fuzzer

```
Predicción de bugs que el fuzzer encontrará:

BUG 1 (segundos): Campo quoted sin límite de longitud
  Input: "aaaa...aaa" (257+ caracteres dentro de quotes)
  ASan: heap-buffer-overflow en csv_parser.c:36
  
BUG 2 (minutos): Demasiados campos por fila
  Input: a,b,c,d,...,z (65+ campos separados por coma)
  ASan: heap-buffer-overflow en csv_parser.c:44
  
BUG 3 (minutos-horas): Muchas filas sin realloc check
  Input: línea\nlínea\n... (17+ filas para exceder capacity inicial)
  Nota: el BUG 3 en realidad SÍ hace realloc, pero tiene BUG 4
  
BUG 4 (depende): realloc retorna NULL (difícil de triggear sin OOM)
  Más probable con ASan por el overhead de memoria
```

### Corregir los bugs

```c
/* csv_parser_fixed.c — versión corregida */

/* ... dentro del loop principal ... */

    if (in_quotes) {
        /* FIX BUG 1: verificar límite del campo */
        if (field_pos < CSV_MAX_FIELD_LEN - 1) {
            table->rows[table->row_count].fields[current_field][field_pos] = c;
            field_pos++;
        }
        /* Si excede, truncar silenciosamente (o retornar error) */
        continue;
    }

    if (c == ',') {
        table->rows[table->row_count].fields[current_field][field_pos] = '\0';
        current_field++;
        field_pos = 0;

        /* FIX BUG 2: verificar límite de campos */
        if (current_field >= CSV_MAX_FIELDS) {
            /* Demasiados campos — truncar o error */
            csv_table_free(table);
            return NULL;
        }
        continue;
    }

    if (c == '\n') {
        /* ... */
        table->row_count++;

        if (table->row_count >= table->capacity) {
            table->capacity *= 2;
            CsvRow *new_rows = realloc(table->rows,
                sizeof(CsvRow) * table->capacity);
            /* FIX BUG 4: verificar realloc */
            if (!new_rows) {
                csv_table_free(table);
                return NULL;
            }
            table->rows = new_rows;
        }
        /* ... */
    }
```

### Crear regression tests

```c
/* test_csv_regressions.c */
#include <assert.h>
#include <string.h>
#include "csv_parser.h"

/* Regression: BUG 1 — campo quoted excede CSV_MAX_FIELD_LEN */
void test_long_quoted_field(void) {
    /* Generar un campo quoted de 500 caracteres */
    char input[503];
    input[0] = '"';
    memset(input + 1, 'A', 500);
    input[501] = '"';
    input[502] = '\0';

    CsvTable *table = csv_parse(input, 502);
    /* Debe parsear sin crashear, campo truncado */
    if (table) {
        assert(strlen(table->rows[0].fields[0]) < CSV_MAX_FIELD_LEN);
        csv_table_free(table);
    }
}

/* Regression: BUG 2 — demasiados campos */
void test_too_many_fields(void) {
    /* Generar 100 campos: "a,a,a,a,...,a" */
    char input[200];
    int pos = 0;
    for (int i = 0; i < 100 && pos < 199; i++) {
        if (i > 0) input[pos++] = ',';
        input[pos++] = 'a';
    }
    input[pos] = '\0';

    CsvTable *table = csv_parse(input, pos);
    /* Debe retornar NULL (demasiados campos) o truncar, no crashear */
    if (table) {
        assert(table->rows[0].field_count <= CSV_MAX_FIELDS);
        csv_table_free(table);
    }
}

int main(void) {
    test_long_quoted_field();
    test_too_many_fields();
    printf("All regression tests passed.\n");
    return 0;
}
```

---

## 28. Programa de práctica: mini_json_parser

Construye un parser de JSON simplificado y fuzzéalo para encontrar bugs.

### Especificación

```
mini_json/
├── Makefile
├── json_parser.h     ← API pública
├── json_parser.c     ← implementación (con bugs intencionales)
├── fuzz_json.c       ← harness para libFuzzer
├── harness_afl.c     ← harness para AFL++
├── corpus/           ← semillas iniciales
│   ├── empty.json
│   ├── null.json
│   ├── number.json
│   ├── string.json
│   ├── array.json
│   ├── object.json
│   └── nested.json
├── json.dict         ← diccionario de tokens JSON
└── tests/
    └── test_json.c   ← unit tests + regression tests
```

### json_parser.h

```c
#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
    JsonType type;
    union {
        int boolean;           /* JSON_BOOL */
        double number;         /* JSON_NUMBER */
        char *string;          /* JSON_STRING (heap-allocated) */
        struct {               /* JSON_ARRAY */
            JsonValue **items;
            int count;
            int capacity;
        } array;
        struct {               /* JSON_OBJECT */
            char **keys;
            JsonValue **values;
            int count;
            int capacity;
        } object;
    };
};

/* Parsea un string JSON y retorna el valor raíz.
 * Retorna NULL si el JSON es inválido.
 * El llamador debe liberar con json_free(). */
JsonValue *json_parse(const char *data, size_t len);
void json_free(JsonValue *value);

#endif
```

### Tareas

1. **Implementa `json_parser.c`** con soporte para los 6 tipos JSON.
   Introduce intencionalmente 3-5 bugs:
   - Al menos un buffer overflow
   - Al menos un caso de recursión sin límite
   - Al menos un manejo incorrecto de strings (escape sequences)
   - Al menos un error de manejo de memoria (leak o double-free)

2. **Escribe `fuzz_json.c`** (harness para libFuzzer):
   - Limitar el tamaño del input a 64KB
   - Parsear y liberar en cada iteración
   - Retornar 0 siempre

3. **Escribe `harness_afl.c`** (harness para AFL++):
   - Leer desde archivo (argv[1]) o stdin
   - Mismo flujo: parsear y liberar

4. **Prepara el corpus** con 7+ semillas que cubran cada tipo JSON

5. **Escribe `json.dict`** con todos los tokens JSON relevantes

6. **Compila y fuzzea**:
   ```bash
   # Con libFuzzer + ASan
   clang -fsanitize=fuzzer,address -g -o fuzz_json fuzz_json.c json_parser.c
   ./fuzz_json -dict=json.dict corpus/
   
   # O con AFL++
   afl-cc -fsanitize=address -o json_afl harness_afl.c json_parser.c
   afl-fuzz -i corpus -o output -x json.dict -- ./json_afl @@
   ```

7. **Documenta los crashes** encontrados:
   - Para cada crash: señal, línea de código, causa raíz
   - Minimiza el input con `afl-tmin` o `-minimize_crash=1`
   - Arregla el bug

8. **Crea regression tests** a partir de los crash files minimizados

9. **Fuzzea la versión corregida** durante al menos 10 minutos:
   - ¿Encuentra nuevos crashes?
   - ¿Cuál es la cobertura final?

---

## 29. Ejercicios

### Ejercicio 1: dumb fuzzer en C

**Objetivo**: Construir un fuzzer aleatorio mínimo para entender los fundamentos.

**Tareas**:

**a)** Implementa un "dumb fuzzer" en C que:
   - Genere archivos de tamaño aleatorio (1-4096 bytes) con contenido aleatorio
   - Ejecute un programa target con cada archivo (via `fork/exec`)
   - Capture la señal de terminación (`WIFSIGNALED`, `WTERMSIG`)
   - Guarde los inputs que causan crashes en un directorio `crashes/`
   - Muestre estadísticas cada 1000 iteraciones (total ejecutados, crashes, velocidad)

**b)** Escribe un programa target simple con un bug:
   ```c
   void process(const char *data, size_t len) {
       char buf[32];
       if (len > 4 && data[0] == 'F' && data[1] == 'U' &&
           data[2] == 'Z' && data[3] == 'Z') {
           memcpy(buf, data, len);  // overflow si len > 32
       }
   }
   ```

**c)** Ejecuta tu dumb fuzzer contra el target. ¿Cuántas iteraciones necesita para
encontrar el crash? Repite 5 veces y calcula el promedio.

**d)** Calcula la probabilidad teórica de generar un input que empiece con "FUZZ"
y tenga más de 32 bytes. Compara con tu resultado empírico.

---

### Ejercicio 2: mutation-based fuzzer

**Objetivo**: Mejorar el dumb fuzzer con mutaciones inteligentes.

**Tareas**:

**a)** Extiende tu dumb fuzzer para que soporte mutation-based fuzzing:
   - Aceptar un directorio de semillas iniciales
   - Implementar al menos 4 operaciones de mutación:
     1. Bit flip (invertir un bit aleatorio)
     2. Byte replacement (reemplazar un byte con valor aleatorio)
     3. Byte insertion (insertar un byte en posición aleatoria)
     4. Interesting values (sustituir 1-4 bytes con 0, 0xFF, 0x7F, 0x80, etc.)
   - Elegir una semilla aleatoria y aplicar 1-3 mutaciones por iteración

**b)** Crea un target más complejo:
   ```c
   void process_v2(const char *data, size_t len) {
       if (len < 8) return;
       if (data[0] != 'M') return;
       if (data[1] != 'A') return;
       if (data[2] != 'G') return;
       if (data[3] != 'I') return;
       if (data[4] != 'C') return;
       uint16_t size = *(uint16_t *)(data + 5);  // bytes 5-6
       if (size > 1000) return;
       char buf[100];
       memcpy(buf, data + 7, size);  // overflow si size > 93
   }
   ```

**c)** Compara tu mutation fuzzer vs el dumb fuzzer:
   - Semilla: archivo que empieza con "MAGIC" y tiene 50 bytes
   - ¿Cuántas iteraciones necesita cada uno para encontrar el crash?
   - ¿Por qué el mutation fuzzer es más rápido?

**d)** Agrega una mutación más: "magic constant replacement" — buscar bytes
consecutivos que coincidan con valores "interesantes" (0x00000000, 0xFFFFFFFF,
0x7FFFFFFF) y sustituirlos. ¿Mejora la detección?

---

### Ejercicio 3: entender coverage-guided fuzzing

**Objetivo**: Comprender intuitivamente por qué el coverage-guided es superior.

**Tareas**:

**a)** Considera esta función:
   ```c
   int deep_check(const char *data, size_t len) {
       if (len < 10) return 0;
       if (data[0] != 'S') return 0;      // gate 1
       if (data[1] != 'E') return 0;      // gate 2
       if (data[2] != 'C') return 0;      // gate 3
       if (data[3] != 'R') return 0;      // gate 4
       if (data[4] != 'E') return 0;      // gate 5
       if (data[5] != 'T') return 0;      // gate 6
       uint32_t key = *(uint32_t *)(data + 6);
       if (key == 0xDEADBEEF) {           // gate 7
           char buf[4];
           memcpy(buf, data, len);        // crash
       }
       return 0;
   }
   ```

   Sin semillas, calcula:
   - Probabilidad de que un dumb fuzzer pase el gate 1: 1/256
   - Probabilidad de pasar gates 1-6: (1/256)^6
   - Probabilidad de pasar gates 1-7: (1/256)^6 × (1/2^32)
   - ¿Cuántas iteraciones en promedio necesitaría un dumb fuzzer?

**b)** Ahora explica paso a paso cómo un coverage-guided fuzzer encontraría el crash:
   - Iteración N: input aleatorio `Sxxxxxx...` pasa gate 1 → nuevo edge → guardado
   - Iteración N+K: mutación de ese input cambia byte 1 a 'E' → pasa gates 1-2 → guardado
   - ... continúa hasta el crash

**c)** Estima cuántas iteraciones necesitaría el coverage-guided fuzzer para cada gate.
Pista: cada gate necesita ~256 mutaciones en promedio (1 byte correcto de 256 posibles).

**d)** Compara: ¿cuántos órdenes de magnitud más rápido es el coverage-guided vs
el dumb fuzzer para esta función? Expresa como ratio.

---

### Ejercicio 4: diseñar corpus y diccionario

**Objetivo**: Practicar la creación de corpus iniciales y diccionarios efectivos.

**Tareas**:

**a)** Para un parser de URLs (`http://user:pass@host:port/path?query#fragment`),
diseña:
   - Un corpus inicial de 8-10 semillas que cubran las variantes:
     - Con/sin scheme, user, password, port, path, query, fragment
     - URLs con caracteres especiales (`%20`, `+`, `&`, `=`)
     - URLs con IPv4 e IPv6
   - Un diccionario con todos los tokens relevantes

**b)** Para un parser de expresiones matemáticas (`2 + 3 * (4 - 1)`), diseña:
   - Corpus inicial de 5-7 semillas
   - Diccionario con operadores, paréntesis, y números especiales

**c)** Explica por qué estas semillas serían malas para un parser de CSV:
   - Un archivo CSV de 1MB con 10,000 filas
   - 50 archivos CSV que son variaciones del mismo template
   - Un archivo CSV con solo un campo por fila

   ¿Qué semillas serían mejores y por qué?

---

## Navegación

- **Anterior**: [C02/S05/T03 - Cobertura realista](../../../C02_Testing_en_Rust/S05_Cobertura_en_Rust/T03_Cobertura_realista/README.md)
- **Siguiente**: [T02 - AFL++](../T02_AFL/README.md)

---

> **Capítulo 3: Fuzzing y Sanitizers** — Sección 1: Fuzzing en C
>
> Tópico 1 de 4 completado
>
> - T01: Concepto de fuzzing ✓
> - T02: AFL++ (siguiente)
> - T03: libFuzzer
> - T04: Escribir harnesses
