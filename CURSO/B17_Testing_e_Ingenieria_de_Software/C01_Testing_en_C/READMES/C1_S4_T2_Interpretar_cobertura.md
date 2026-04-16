# T02 — Interpretar cobertura: líneas vs ramas vs condiciones

> **Bloque 17 — Testing e Ingeniería de Software → C01 — Testing en C → S04 — Cobertura en C → T02**

---

## Índice

1. [El problema de leer cobertura sin criterio](#1-el-problema-de-leer-cobertura-sin-criterio)
2. [Métricas de cobertura en profundidad](#2-métricas-de-cobertura-en-profundidad)
3. [Line coverage: la métrica más común y más engañosa](#3-line-coverage-la-métrica-más-común-y-más-engañosa)
4. [Function coverage: la métrica más gruesa](#4-function-coverage-la-métrica-más-gruesa)
5. [Branch coverage: decisiones binarias](#5-branch-coverage-decisiones-binarias)
6. [Condition coverage: sub-expresiones booleanas](#6-condition-coverage-sub-expresiones-booleanas)
7. [MC/DC: Modified Condition/Decision Coverage](#7-mcdc-modified-conditiondecision-coverage)
8. [Path coverage: el ideal inalcanzable](#8-path-coverage-el-ideal-inalcanzable)
9. [Jerarquía de métricas: qué implica qué](#9-jerarquía-de-métricas-qué-implica-qué)
10. [Por qué 100% de cobertura no significa correcto](#10-por-qué-100-de-cobertura-no-significa-correcto)
11. [Anatomía de un reporte: leer gcov línea por línea](#11-anatomía-de-un-reporte-leer-gcov-línea-por-línea)
12. [Anatomía de un reporte: leer lcov/genhtml](#12-anatomía-de-un-reporte-leer-lcovgenhtml)
13. [Patrones de código problemático para cobertura](#13-patrones-de-código-problemático-para-cobertura)
14. [Cobertura y calidad de tests: la distinción crítica](#14-cobertura-y-calidad-de-tests-la-distinción-crítica)
15. [Umbrales de cobertura: cuánto es suficiente](#15-umbrales-de-cobertura-cuánto-es-suficiente)
16. [Anti-patrones: gaming the coverage](#16-anti-patrones-gaming-the-coverage)
17. [Cobertura como herramienta de diseño](#17-cobertura-como-herramienta-de-diseño)
18. [Comparación de interpretación: C vs Rust vs Go](#18-comparación-de-interpretación-c-vs-rust-vs-go)
19. [Ejemplo completo: auditoría de cobertura de un parser](#19-ejemplo-completo-auditoría-de-cobertura-de-un-parser)
20. [Programa de práctica](#20-programa-de-práctica)
21. [Ejercicios](#21-ejercicios)

---

## 1. El problema de leer cobertura sin criterio

Un equipo ejecuta sus tests y obtiene este reporte:

```
$ lcov --summary filtered.info
Summary coverage rate:
  lines......: 87.3% (412 of 472)
  functions..: 93.1% (54 of 58)
  branches...: 71.2% (178 of 250)
```

La reacción típica: "87% de líneas, estamos bien". Pero este número, sin contexto, puede significar cualquier cosa:

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  El mismo 87% de line coverage puede indicar:                   ║
 ║                                                                  ║
 ║  Escenario A: El 13% no cubierto es código de logging y         ║
 ║  formateo de debug → cobertura EXCELENTE, el core está sólido   ║
 ║                                                                  ║
 ║  Escenario B: El 13% no cubierto son los paths de error         ║
 ║  de parsing, validación de entrada y manejo de memoria →        ║
 ║  cobertura PELIGROSA, justo lo crítico no está testeado         ║
 ║                                                                  ║
 ║  Escenario C: El 87% se logró con tests que ejecutan el         ║
 ║  código pero nunca verifican resultados → cobertura INÚTIL,     ║
 ║  los tests no detectan nada                                      ║
 ╚══════════════════════════════════════════════════════════════════╝
```

### Qué necesitas para interpretar cobertura

```
Dato                            Por qué importa
──────────────────────────────────────────────────────────────────────────
Qué métrica estás mirando       Line ≠ branch ≠ condition ≠ path
Qué código queda descubierto    El % importa menos que el QUÉ
Por qué está descubierto        ¿Intencionalmente? ¿Difícil de testear?
Calidad de los tests            ¿Los tests VERIFICAN o solo EJECUTAN?
Contexto del proyecto           ¿Safety-critical? ¿Prototipo? ¿Producción?
Tendencia temporal              ¿La cobertura sube o baja con cada commit?
```

Interpretar cobertura es un **acto de análisis**, no de lectura de un número. Este tópico enseña ese análisis.

---

## 2. Métricas de cobertura en profundidad

### Las seis métricas principales

```
  Nivel 1 — Statement/Line Coverage
  ┌────────────────────────────────────────────────────────────────┐
  │  ¿Se ejecutó esta línea al menos una vez?                      │
  │  Herramienta: gcov (default), lcov                             │
  │  Complejidad: Baja                                             │
  │  Utilidad: Punto de partida, no destino                        │
  └────────────────────────────────────────────────────────────────┘

  Nivel 2 — Function Coverage
  ┌────────────────────────────────────────────────────────────────┐
  │  ¿Se llamó esta función al menos una vez?                      │
  │  Herramienta: gcov -f, lcov                                   │
  │  Complejidad: Muy baja                                         │
  │  Utilidad: Detectar funciones completamente olvidadas          │
  └────────────────────────────────────────────────────────────────┘

  Nivel 3 — Branch Coverage (Decision Coverage)
  ┌────────────────────────────────────────────────────────────────┐
  │  ¿Cada decisión (if, while, switch) fue true Y false?         │
  │  Herramienta: gcov -b, lcov --rc lcov_branch_coverage=1       │
  │  Complejidad: Media                                            │
  │  Utilidad: Detectar ramas never-taken                          │
  └────────────────────────────────────────────────────────────────┘

  Nivel 4 — Condition Coverage
  ┌────────────────────────────────────────────────────────────────┐
  │  ¿Cada sub-expresión booleana fue true Y false?               │
  │  Herramienta: BullseyeCoverage, Parasoft (no gcov)            │
  │  Complejidad: Alta                                             │
  │  Utilidad: Verificar condiciones compuestas                    │
  └────────────────────────────────────────────────────────────────┘

  Nivel 5 — MC/DC (Modified Condition/Decision Coverage)
  ┌────────────────────────────────────────────────────────────────┐
  │  ¿Cada sub-expresión puede cambiar el resultado               │
  │   independientemente de las demás?                             │
  │  Herramienta: Especializada (VectorCAST, LDRA, Parasoft)      │
  │  Complejidad: Muy alta                                         │
  │  Utilidad: Safety-critical (DO-178C Level A, ISO 26262 ASIL D)│
  └────────────────────────────────────────────────────────────────┘

  Nivel 6 — Path Coverage
  ┌────────────────────────────────────────────────────────────────┐
  │  ¿Se ejecutó cada camino posible a través del código?         │
  │  Herramienta: Teórica (explosión combinatoria)                │
  │  Complejidad: Extrema (a menudo infinita)                      │
  │  Utilidad: Ideal teórico, no práctico                          │
  └────────────────────────────────────────────────────────────────┘
```

### Tabla comparativa rápida

```
Métrica     Pregunta clave           Ejemplo if(a && b)      gcov soporta
────────────────────────────────────────────────────────────────────────────
Line        ¿Se ejecutó la línea?    Sí, con a=1,b=1         ✓
Function    ¿Se llamó la función?    Sí                       ✓ (gcov -f)
Branch      ¿if fue T y F?          T con a=1,b=1;           ✓ (gcov -b)
                                      F con a=0               
Condition   ¿a fue T y F?           Necesita 4 combos        ✗
            ¿b fue T y F?                                    
MC/DC       ¿a sola cambia result?  Mín 3 tests              ✗
Path        ¿Todos los caminos?     Crece exponencialmente    ✗
```

---

## 3. Line coverage: la métrica más común y más engañosa

### Definición formal

```
Line Coverage = (Líneas ejecutadas al menos una vez) / (Total de líneas ejecutables) × 100%
```

gcov marca como ejecutable toda línea que genera código máquina. No son ejecutables:
- Comentarios
- Líneas en blanco
- Declaraciones `#include`, `#define`, `typedef`
- Llaves sueltas `{` y `}` (depende del compilador)
- Declaraciones de variables sin inicialización (a veces)

### Ejemplo: 100% de line coverage con bug no detectado

```c
// account.c
#include <stdbool.h>

typedef struct {
    double balance;
    bool   frozen;
} Account;

// Transferir dinero de src a dst
bool transfer(Account *src, Account *dst, double amount) {
    if (amount <= 0) return false;
    if (src->frozen || dst->frozen) return false;
    if (src->balance < amount) return false;

    src->balance -= amount;
    dst->balance += amount;
    return true;
}
```

```c
// test_account.c
#include <assert.h>
#include "account.c"

int main(void) {
    Account a = {1000.0, false};
    Account b = {500.0, false};

    // Test 1: transferencia válida
    assert(transfer(&a, &b, 200.0));
    assert(a.balance == 800.0);
    assert(b.balance == 700.0);

    // Test 2: monto negativo
    assert(!transfer(&a, &b, -50.0));

    // Test 3: cuenta congelada
    a.frozen = true;
    assert(!transfer(&a, &b, 100.0));
    a.frozen = false;

    // Test 4: fondos insuficientes
    assert(!transfer(&a, &b, 9999.0));

    return 0;
}
```

```bash
$ gcc -O0 -g --coverage -o test_account test_account.c
$ ./test_account
$ gcov -f account.c
Function 'transfer'
Lines executed:100.00% of 7
```

100% de line coverage. Pero hay bugs que **no se detectan**:

```c
// Bug 1: ¿Qué pasa si src == dst? (auto-transferencia)
// transfer(&a, &a, 100.0) duplicaría dinero:
// a.balance -= 100 → 900
// a.balance += 100 → 1000  ← ¡El balance no cambió!
// Pero la función dice que se transfirió exitosamente

// Bug 2: ¿Qué pasa si src o dst es NULL?
// transfer(NULL, &b, 100.0) → CRASH (segfault)
// No hay guard para NULL

// Bug 3: ¿Qué pasa con overflow?
// Si dst->balance + amount > DBL_MAX → comportamiento indefinido

// Bug 4: ¿Concurrencia? Si dos threads transfieren simultáneamente
// desde la misma cuenta, pueden extraer más de lo disponible
```

### Diagramas: qué ve y qué no ve line coverage

```
  transfer(src, dst, amount)

  ┌─────────────────────────────────────────────┐
  │                                             │
  │  if (amount <= 0)  ── true ──► return false │  ← CUBIERTA (T2)
  │         │                                   │
  │       false                                 │
  │         ▼                                   │
  │  if (frozen)       ── true ──► return false │  ← CUBIERTA (T3)
  │         │                                   │
  │       false                                 │
  │         ▼                                   │
  │  if (balance < amt)── true ──► return false │  ← CUBIERTA (T4)
  │         │                                   │
  │       false                                 │
  │         ▼                                   │
  │  src -= amount                              │  ← CUBIERTA (T1)
  │  dst += amount                              │  ← CUBIERTA (T1)
  │  return true                                │  ← CUBIERTA (T1)
  │                                             │
  └─────────────────────────────────────────────┘

  CUBIERTO: 7/7 líneas = 100%

  NO VERIFICADO:
  ❌ src == dst (auto-transferencia)
  ❌ src == NULL o dst == NULL
  ❌ overflow en balance
  ❌ thread safety
  ❌ amount == 0.0 exactamente
  ❌ amount con valor NaN o Inf

  Line coverage no ve ENTRADAS,
  solo ve CÓDIGO EJECUTADO.
```

### Lo que line coverage sí es bueno para

```
Uso legítimo de line coverage       Explicación
──────────────────────────────────────────────────────────────────
Encontrar código muerto             Si una función tiene 0%, nunca se llama
Detectar tests faltantes            Si un módulo tiene 0%, nadie lo testea
Medir progreso incremental          "Subimos de 40% a 60%" (tendencia)
Identificar ramas error olvidadas   ###### en el path de error
Requisito mínimo de CI              "No bajar de 70%"
```

```
Uso INCORRECTO de line coverage     Por qué
──────────────────────────────────────────────────────────────────
"87% = código seguro"               No indica calidad de verificación
"100% = no hay bugs"                Solo indica ejecución, no corrección
"Más % siempre mejor"               Rendimientos decrecientes pasado ~85%
"Medir productividad del dev"       Incentiva tests vacíos
```

---

## 4. Function coverage: la métrica más gruesa

### Definición

```
Function Coverage = (Funciones llamadas al menos una vez) / (Total de funciones) × 100%
```

### Qué revela

Function coverage es útil como **primer filtro**: si una función tiene 0 llamadas, no tiene ningún test. Eso es una señal inmediata.

```c
// file_utils.c — 5 funciones

size_t file_size(const char *path);        // Llamada en tests ✓
char  *file_read(const char *path);        // Llamada en tests ✓
bool   file_write(const char *path, ...);  // Llamada en tests ✓
bool   file_copy(const char *src, ...);    // NUNCA llamada    ✗
bool   file_delete(const char *path);      // NUNCA llamada    ✗
```

```bash
$ gcov -f file_utils.c
Function 'file_size'
Lines executed:100.00% of 8

Function 'file_read'
Lines executed:75.00% of 12

Function 'file_write'
Lines executed:66.67% of 9

Function 'file_copy'
Lines executed:0.00% of 15        ← ALERTA

Function 'file_delete'
Lines executed:0.00% of 6         ← ALERTA

File 'file_utils.c'
Lines executed:60.00% of 50
```

### Limitaciones

Function coverage solo dice "se llamó o no". No dice:
- Cuántas líneas dentro de la función se ejecutaron
- Con qué argumentos se llamó
- Si el resultado fue verificado

Una función con 100% de function coverage puede tener 10% de line coverage:

```c
void process_request(Request *req) {          // Llamada ✓ (function cov = 100%)
    if (req->type == REQ_GET) {               // Ejecutada ✓
        handle_get(req);                       // Ejecutada ✓
    } else if (req->type == REQ_POST) {       //
        validate_body(req);                    // ##### NUNCA
        handle_post(req);                      // ##### NUNCA
    } else if (req->type == REQ_DELETE) {     //
        check_permissions(req);                // ##### NUNCA
        handle_delete(req);                    // ##### NUNCA
    } else {                                  //
        log_error("Unknown request type");     // ##### NUNCA
        send_400(req);                         // ##### NUNCA
    }
}
// Function coverage: 100% (se llamó la función)
// Line coverage: 30% (solo ejecutó el path GET)
```

### Cuándo es útil function coverage

```
Escenario                                Utilidad
──────────────────────────────────────────────────────────────────
Auditoría inicial de tests               Alta — encontrar módulos sin tests
Verificar que API pública tiene tests    Alta — cada función exportada
Medir cobertura de biblioteca            Media — ¿se testean todas las funciones?
Evaluar calidad detallada                Baja — demasiado gruesa
```

---

## 5. Branch coverage: decisiones binarias

### Definición formal

```
Branch Coverage = (Ramas ejecutadas) / (Total de ramas posibles) × 100%
```

Una **rama** (branch) es cada posible resultado de una **decisión** (decision point). Cada `if`, `while`, `for`, `switch`, `? :` genera al menos dos ramas.

### Decisiones y sus ramas

```
Construcción         Ramas generadas
──────────────────────────────────────────────────────────────────
if (expr)            2 ramas: true, false
if/else              2 ramas: true, false (mismas)
if/else if/else      2 ramas por cada if
while (expr)         2 ramas: continuar, salir
for (;;expr;;)       2 ramas: continuar, salir
do {} while(expr)    2 ramas: continuar, salir
switch (N cases)     N+1 ramas: N cases + default (implícito o explícito)
expr ? a : b         2 ramas: true, false
&&                   2 ramas por short-circuit
||                   2 ramas por short-circuit
```

### Ejemplo detallado

```c
int categorize(int value, bool strict) {
    if (value < 0) {                    // Decisión 1: 2 ramas
        return -1;
    }

    if (strict && value == 0) {         // Decisión 2: genera 3 ramas por short-circuit
        return 0;                       //   branch 0: strict=false (short-circuit)
    }                                   //   branch 1: strict=true, value==0
                                        //   branch 2: strict=true, value!=0

    if (value <= 100) {                 // Decisión 3: 2 ramas
        return 1;
    } else if (value <= 1000) {         // Decisión 4: 2 ramas
        return 2;
    }

    return 3;
}
```

Ramas totales: 2 + 3 + 2 + 2 = **9 ramas**.

### gcov -b output para este ejemplo

```bash
$ gcov -b categorize.c
```

Supongamos que solo testeamos con `categorize(50, false)`:

```
        1:    1:int categorize(int value, bool strict) {
        1:    2:    if (value < 0) {
branch  0 taken 0%                    ← NUNCA: value < 0
branch  1 taken 100%                  ← SIEMPRE: value >= 0
    #####:    3:        return -1;
        -:    4:    }
        -:    5:
        1:    6:    if (strict && value == 0) {
branch  0 taken 0%                    ← NUNCA: strict=true
branch  1 taken 0%                    ← NUNCA: strict && value==0
branch  2 taken 100%                  ← SIEMPRE: !strict (short-circuit)
    #####:    7:        return 0;
        -:    8:    }
        -:    9:
        1:   10:    if (value <= 100) {
branch  0 taken 100%                  ← SIEMPRE: value <= 100
branch  1 taken 0%                    ← NUNCA: value > 100
        1:   11:        return 1;
    #####:   12:    } else if (value <= 1000) {
branch  0 never executed
branch  1 never executed
    #####:   13:        return 2;
        -:   14:    }
        -:   15:
    #####:   16:    return 3;
        -:   17:}
```

```
Análisis:
  Line coverage:    42.9%  (3 de 7 ejecutables)
  Branch coverage:  33.3%  (3 de 9 ramas)

  Ramas cubiertas:
    ✓ value >= 0 (D1 false)
    ✓ !strict short-circuit (D2 branch 2)
    ✓ value <= 100 (D3 true)

  Ramas NO cubiertas:
    ✗ value < 0 (D1 true)
    ✗ strict=true, value==0 (D2 branch 1)
    ✗ strict=true, value!=0 (D2 branch 0)
    ✗ value > 100 (D3 false)
    ✗ value <= 1000 (D4 true)
    ✗ value > 1000 (D4 false)
```

### Tests para 100% branch coverage

```c
void test_all_branches(void) {
    assert(categorize(-5, false) == -1);     // D1 true
    assert(categorize(50, false) == 1);      // D1 false, D2 short-circuit, D3 true
    assert(categorize(0, true) == 0);        // D2 strict=true, value==0
    assert(categorize(50, true) == 1);       // D2 strict=true, value!=0 → D3 true
    assert(categorize(500, false) == 2);     // D3 false, D4 true
    assert(categorize(5000, false) == 3);    // D4 false
}
```

### Branch coverage vs line coverage: la diferencia visible

```c
// Con este código:
int safe_divide(int a, int b) {
    if (b == 0) return 0;
    return a / b;
}

// Test: safe_divide(10, 2) → 5
// Line coverage:  66.7% (return 0 no ejecutado, pero if sí)
// Branch coverage: 50.0% (solo b != 0 cubierto)

// Test: safe_divide(10, 0) y safe_divide(10, 2)
// Line coverage:  100%
// Branch coverage: 100%
```

La diferencia importa cuando las ramas protegen contra errores:

```c
char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;                   // Rama error: ¿se testea?

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {                        // Rama error: ¿se testea?
        fclose(f);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) {                            // Rama error: ¿se testea?
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, size, f);
    fclose(f);

    if ((long)read != size) {              // Rama error: ¿se testea?
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    return buf;
}
```

Con solo tests del happy path:

```
Line coverage:    58%  (7 de 12 líneas ejecutables — se "ve" bajo)
Branch coverage:  50%  (4 de 8 ramas — solo las ramas "felices")

Las 4 ramas de error NO cubiertas son justo las que
protegen contra:
  - Archivo inexistente
  - Error de ftell
  - Error de malloc (OOM)
  - Lectura parcial

Branch coverage revela esto. Line coverage solo dice "58%".
```

---

## 6. Condition coverage: sub-expresiones booleanas

### Definición

```
Condition Coverage = (Sub-expresiones evaluadas a true + sub-expresiones evaluadas a false)
                     / (2 × total de sub-expresiones)
                     × 100%
```

Condition coverage exige que cada **átomo booleano** (cada sub-expresión que no contiene operadores lógicos) se evalúe tanto a `true` como a `false`.

### Diferencia con branch coverage

```c
if (a > 0 && b > 0) {
    // ...
}
```

**Branch coverage** requiere:
- El `if` completo sea `true` al menos una vez
- El `if` completo sea `false` al menos una vez
- Ejemplo: `a=1,b=1` (true) y `a=0,b=0` (false) → 100% branch coverage

**Condition coverage** requiere:
- `a > 0` sea `true` y `false`
- `b > 0` sea `true` y `false`
- Ejemplo: `a=1,b=0` y `a=0,b=1` → 100% condition coverage
- ¡Pero `a=1,b=0` es false y `a=0,b=1` también es false!
- → 0% branch coverage con 100% condition coverage

### Esto significa que condition coverage y branch coverage son independientes

```
                        Branch = 100%   Branch < 100%
                       ──────────────  ──────────────
  Condition = 100%   │  IDEAL          POSIBLE (!)
  Condition < 100%   │  POSIBLE (!)    POSIBLE
```

Ejemplos:

```
Caso 1: 100% Branch, 0% Condition
  if (a && b)
  Tests: (a=1,b=1) → true,  (a=0,b=?) → false
  Branch: ✓ true, ✓ false → 100%
  Condition: a fue true+false ✓, pero b solo fue true (short-circuit) → < 100%

Caso 2: 100% Condition, 0% Branch
  if (a && b)
  Tests: (a=1,b=0) → false,  (a=0,b=1) → false
  Condition: a fue true+false ✓, b fue true+false (pero wait... 
             con a=0, b nunca se evalúa por short-circuit)
  Realmente: solo funciona si la herramienta ignora short-circuit
```

### Por qué gcov no mide condition coverage

gcov trabaja a nivel del **grafo de flujo de control** (CFG) generado por el compilador. El compilador convierte `if (a && b)` en:

```
                 ┌──────────┐
                 │ eval: a  │
                 └────┬─────┘
                      │
              ┌───────┴────────┐
         true │                │ false
              ▼                ▼
        ┌──────────┐    ┌──────────────┐
        │ eval: b  │    │ if = false   │
        └────┬─────┘    │ (skip body)  │
             │          └──────────────┘
     ┌───────┴────────┐
true │                │ false
     ▼                ▼
┌──────────┐   ┌──────────────┐
│ if = true│   │ if = false   │
│ (body)   │   │ (skip body)  │
└──────────┘   └──────────────┘
```

gcov cuenta arcos entre bloques, no valores de sub-expresiones. Puede contar 3 ramas (a=false, b=false, b=true) pero no que "a fue true 5 veces y false 2 veces".

### Herramientas que miden condition coverage

```
Herramienta         Tipo          Condition   MC/DC   Plataforma
──────────────────────────────────────────────────────────────────
BullseyeCoverage    Comercial     ✓           ✓       Windows/Linux
VectorCAST          Comercial     ✓           ✓       Embedded/Desktop
LDRA TBrun          Comercial     ✓           ✓       Safety-critical
Parasoft C/C++test  Comercial     ✓           ✓       Enterprise
Cantata             Comercial     ✓           ✓       Automotive
gcov                Libre         ✗           ✗       Cualquiera
llvm-cov            Libre         Parcial     ✗       Cualquiera
```

---

## 7. MC/DC: Modified Condition/Decision Coverage

### Definición

MC/DC exige que se demuestre que **cada sub-expresión booleana puede, por sí sola, cambiar el resultado de la decisión completa**, manteniendo las demás condiciones constantes.

### Requisitos formales de MC/DC

Para una decisión con N condiciones, MC/DC requiere:

1. **Decision coverage**: la decisión completa fue true y false
2. **Condition coverage**: cada condición fue true y false
3. **Independence**: para cada condición, hay un par de tests donde:
   - Solo esa condición cambia
   - Y el resultado de la decisión cambia

### Ejemplo con dos condiciones

```c
if (a && b) {
    action();
}
```

Tabla de verdad completa (4 filas):

```
Test   a       b       a && b     Necesario para MC/DC?
──────────────────────────────────────────────────────────
1      false   false   false      
2      false   true    false      Sí: par con 4 para b (b cambia, resultado no... wait)
3      true    false   false      Sí: par con 4 para a — NO. Par con 4 para b.
4      true    true    true       Sí: siempre necesario (único true)
```

Para MC/DC de `a && b` necesitamos **mínimo 3 tests** (N+1 para N condiciones):

```
Independencia de 'a':
  Necesitamos dos tests donde:
  - 'b' es constante (true)
  - 'a' cambia: true→false o false→true
  - El resultado de la decisión cambia

  Test 4: a=true,  b=true  → true
  Test 2: a=false, b=true  → false   ← 'a' cambió el resultado

Independencia de 'b':
  Necesitamos dos tests donde:
  - 'a' es constante (true)
  - 'b' cambia
  - El resultado cambia

  Test 4: a=true,  b=true  → true
  Test 3: a=true,  b=false → false   ← 'b' cambió el resultado

Tests MC/DC mínimos: {2, 3, 4} = 3 tests (de 4 posibles)
```

### Ejemplo con tres condiciones

```c
if (engine_on && fuel_ok && altitude > min_altitude) {
    engage_autopilot();
}
```

```
Condiciones: A = engine_on, B = fuel_ok, C = altitude > min_altitude

Tabla de verdad (8 filas, solo mostramos las necesarias para MC/DC):

Test   A      B      C      Decision   Demuestra
──────────────────────────────────────────────────────────
T1     true   true   true   true       Base (todas true)
T2     false  true   true   false      Independencia de A
T3     true   false  true   false      Independencia de B
T4     true   true   false  false      Independencia de C

MC/DC: 4 tests (N+1 = 3+1)

T2 vs T1: Solo A cambió → resultado cambió ✓ (A es independiente)
T3 vs T1: Solo B cambió → resultado cambió ✓ (B es independiente)
T4 vs T1: Solo C cambió → resultado cambió ✓ (C es independiente)
```

### Por qué MC/DC importa en safety-critical

```
Estándar              Dominio          Nivel de cobertura requerido
──────────────────────────────────────────────────────────────────────
DO-178C Level A       Aviónica         MC/DC obligatorio
DO-178C Level B       Aviónica         Decision coverage (branch)
DO-178C Level C       Aviónica         Statement coverage (line)
ISO 26262 ASIL D      Automotive       MC/DC recomendado
ISO 26262 ASIL B      Automotive       Branch coverage
IEC 62304 Class C     Medical          Branch coverage
IEC 61508 SIL 4       Industrial       MC/DC recomendado
```

### MC/DC en C sin herramientas comerciales

Si no puedes pagar herramientas, puedes aplicar MC/DC **manualmente** descomponiendo condiciones:

```c
// En vez de:
if (a && b && c) { ... }

// Descomponer para que branch coverage ≈ MC/DC:
bool cond_a = (engine_on);
bool cond_b = (fuel_ok);
bool cond_c = (altitude > min_altitude);

if (cond_a) {
    if (cond_b) {
        if (cond_c) {
            engage_autopilot();
        }
    }
}
```

Ahora branch coverage en gcov cubre cada condición por separado, lo cual es cercano a MC/DC (aunque no idéntico — MC/DC también requiere demostrar independencia).

---

## 8. Path coverage: el ideal inalcanzable

### Definición

```
Path Coverage = (Caminos ejecutados) / (Total de caminos posibles) × 100%
```

Un **camino** (path) es una secuencia completa de ejecución desde la entrada hasta la salida de una función, incluyendo qué rama se tomó en cada decisión.

### Explosión combinatoria

```c
void process(int a, int b, int c, int d) {
    if (a > 0) { /* ... */ }    // 2 ramas
    if (b > 0) { /* ... */ }    // 2 ramas
    if (c > 0) { /* ... */ }    // 2 ramas
    if (d > 0) { /* ... */ }    // 2 ramas
}
```

```
Paths = 2 × 2 × 2 × 2 = 16 caminos

Con 10 decisiones independientes: 2^10 = 1,024 caminos
Con 20 decisiones independientes: 2^20 = 1,048,576 caminos
Con loops: infinito (cada iteración es un path diferente)
```

### Diagrama de paths

```c
void example(int x) {
    if (x > 0) {        // D1
        printf("pos");
    }
    if (x % 2 == 0) {   // D2
        printf("even");
    }
}
```

```
                  ┌────────┐
                  │ ENTRADA │
                  └───┬────┘
                      │
                 ┌────┴────┐
            D1:  │ x > 0?  │
                 └────┬────┘
                      │
              ┌───────┴───────┐
         true │               │ false
              ▼               ▼
        ┌───────────┐   ┌───────────┐
        │ "pos"     │   │  (nada)   │
        └─────┬─────┘   └─────┬─────┘
              │               │
              └───────┬───────┘
                      │
                 ┌────┴────┐
            D2:  │ x%2==0? │
                 └────┬────┘
                      │
              ┌───────┴───────┐
         true │               │ false
              ▼               ▼
        ┌───────────┐   ┌───────────┐
        │ "even"    │   │  (nada)   │
        └─────┬─────┘   └─────┬─────┘
              │               │
              └───────┬───────┘
                      │
                  ┌───┴───┐
                  │ SALIDA │
                  └───────┘

  4 Paths:
  Path 1: x=2   → D1 true,  D2 true   "pos" + "even"
  Path 2: x=3   → D1 true,  D2 false  "pos"
  Path 3: x=-2  → D1 false, D2 true   "even"
  Path 4: x=-3  → D1 false, D2 false  (nada)
```

Para 100% path coverage se necesitan 4 tests. Pero branch coverage requiere solo 2 (un test con D1=true, D2=true; otro con D1=false, D2=false).

### Path coverage con loops

```c
void repeat(int n) {
    for (int i = 0; i < n; i++) {
        printf(".");
    }
}
```

```
Paths posibles:
  n ≤ 0: 0 iteraciones (1 path)
  n = 1: 1 iteración (1 path)
  n = 2: 2 iteraciones (1 path)
  n = 3: 3 iteraciones (1 path)
  ...
  n = k: k iteraciones (1 path)
  ...
  → INFINITOS paths
```

### Por qué path coverage es teórico

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  Path coverage es el estándar teórico ideal pero:               ║
 ║                                                                  ║
 ║  1. Infinito con loops → imposible 100%                         ║
 ║  2. Exponencial sin loops → impráctico con >10 decisiones       ║
 ║  3. Muchos paths son imposibles (condiciones correlacionadas)   ║
 ║  4. Ninguna herramienta lo implementa completamente             ║
 ║                                                                  ║
 ║  En la práctica: MC/DC es el estándar más estricto alcanzable.  ║
 ║  Para la mayoría de proyectos: branch coverage es suficiente.   ║
 ╚══════════════════════════════════════════════════════════════════╝
```

---

## 9. Jerarquía de métricas: qué implica qué

### Diagrama de implicación

```
              ┌──────────────────────────────────────────────────────┐
              │                                                      │
              │              PATH COVERAGE                           │
              │         (cada camino completo)                       │
              │              │                                       │
              │              │ implica                               │
              │              ▼                                       │
              │           MC/DC                                      │
              │    (independencia de condiciones)                     │
              │              │                                       │
              │              │ implica                               │
              │              ▼                                       │
              │   ┌──────────┴──────────┐                           │
              │   │                     │                            │
              │   ▼                     ▼                            │
              │  CONDITION           BRANCH/DECISION                 │
              │  COVERAGE            COVERAGE                        │
              │   │                     │                            │
              │   │                     │ implica                    │
              │   │                     ▼                            │
              │   │               LINE/STATEMENT                     │
              │   │               COVERAGE                           │
              │   │                     │                            │
              │   │                     │ implica                    │
              │   │                     ▼                            │
              │   │               FUNCTION                           │
              │   │               COVERAGE                           │
              │   │                                                  │
              │   │   ╔═══════════════════════════════════╗          │
              │   └──►║  Condition y Branch/Decision      ║          │
              │       ║  son INDEPENDIENTES entre sí.     ║          │
              │       ║  Ninguno implica al otro.         ║          │
              │       ╚═══════════════════════════════════╝          │
              │                                                      │
              └──────────────────────────────────────────────────────┘
```

### Tabla de implicación

```
Si tienes 100% de...     Entonces garantizas:
──────────────────────────────────────────────────────────────
Path coverage         → MC/DC ✓, Condition ✓, Branch ✓, Line ✓, Function ✓
MC/DC                 → Branch ✓, Condition ✓, Line ✓, Function ✓
Branch coverage       → Line ✓, Function ✓ (pero NO Condition)
Condition coverage    → (ninguna otra garantizada automáticamente)
Line coverage         → Function ✓ (parcial — si se ejecuta al menos 1 línea)
Function coverage     → (ninguna otra)
```

### Ejemplo que demuestra la independencia condition vs branch

```c
if (a || b) {
    action();
}
```

```
Tests para 100% branch + 0% condition (conceptual):
  Test 1: a=true,  b=?    → true  (branch true cubierta, b nunca evaluada)
  Test 2: a=false, b=false → false (branch false cubierta)
  Branch: 100% (true + false)
  Condition: a fue true+false ✓, pero b solo fue false (nunca true) → <100%

Tests para 100% condition + <100% branch:
  Test 1: a=true,  b=false → true
  Test 2: a=false, b=true  → true
  Condition: a=T+F ✓, b=T+F ✓ → 100%
  Branch: solo true (nunca false) → 50%

  (Para branch false, necesitamos a=false, b=false)
```

---

## 10. Por qué 100% de cobertura no significa correcto

Este es el concepto más importante de todo el tópico. Vamos a demostrarlo con múltiples ejemplos progresivos.

### Ejemplo 1: test que ejecuta pero no verifica

```c
// sort.c
void bubble_sort(int *arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }
}

// test_sort.c — 100% line coverage, 0% utilidad
void test_sort(void) {
    int arr[] = {3, 1, 4, 1, 5};
    bubble_sort(arr, 5);
    // No verifica NADA
    // ¿Quedó ordenado? ¿Modificó n elementos? ¿Crasheó?
    // La cobertura dice 100%, el test no detecta nada.
}
```

### Ejemplo 2: test que verifica pero no cubre todos los escenarios

```c
// Implementación con bug sutil
int binary_search(const int *arr, int n, int target) {
    int low = 0, high = n - 1;
    while (low <= high) {
        int mid = (low + high) / 2;   // BUG: overflow si low+high > INT_MAX
        if (arr[mid] == target) return mid;
        if (arr[mid] < target) low = mid + 1;
        else high = mid - 1;
    }
    return -1;
}

// Tests con 100% line y branch coverage
void test_binary_search(void) {
    int arr[] = {1, 3, 5, 7, 9};

    assert(binary_search(arr, 5, 5) == 2);   // Encontrar medio
    assert(binary_search(arr, 5, 1) == 0);   // Encontrar primero
    assert(binary_search(arr, 5, 9) == 4);   // Encontrar último
    assert(binary_search(arr, 5, 4) == -1);  // No encontrar

    // 100% line coverage ✓
    // 100% branch coverage ✓
    // Bug no detectado: mid overflow con arrays grandes
    // El fix correcto es: int mid = low + (high - low) / 2;
}
```

### Ejemplo 3: cobertura no verifica efectos secundarios

```c
typedef struct {
    int    items[100];
    int    count;
    double total_value;
} Cart;

void add_to_cart(Cart *cart, int item_id, double price) {
    if (cart->count >= 100) return;
    cart->items[cart->count] = item_id;
    cart->count++;
    cart->total_value += price;
}

void remove_from_cart(Cart *cart, int index) {
    if (index < 0 || index >= cart->count) return;

    // BUG: no actualiza total_value
    for (int i = index; i < cart->count - 1; i++) {
        cart->items[i] = cart->items[i + 1];
    }
    cart->count--;
    // Falta: cart->total_value -= precio del item eliminado
}

// Test con 100% coverage
void test_cart(void) {
    Cart cart = {0};

    add_to_cart(&cart, 1, 10.0);
    add_to_cart(&cart, 2, 20.0);
    assert(cart.count == 2);               // ✓ Verifica count

    remove_from_cart(&cart, 0);
    assert(cart.count == 1);               // ✓ Verifica count
    assert(cart.items[0] == 2);            // ✓ Verifica reordenamiento

    // NO verifica: cart.total_value debería ser 20.0 pero sigue en 30.0
    // 100% coverage, bug no detectado
}
```

### Ejemplo 4: valores límite no cubiertos

```c
// Implementación correcta... excepto en los límites
uint8_t percent_to_byte(int percent) {
    if (percent < 0)   return 0;
    if (percent > 100) return 255;
    return (uint8_t)(percent * 255 / 100);
}

// Tests con 100% coverage
void test_percent(void) {
    assert(percent_to_byte(-5) == 0);
    assert(percent_to_byte(50) == 127);    // 50*255/100 = 127 (truncated)
    assert(percent_to_byte(150) == 255);

    // No testea:
    // percent_to_byte(0) → ¿es 0?
    // percent_to_byte(100) → ¿es 255?
    // percent_to_byte(1) → ¿es 2 o 3? (rounding)
    // percent_to_byte(99) → ¿es 252 o 253?
    // percent_to_byte(INT_MIN) → ¿funciona?
    // percent_to_byte(INT_MAX) → ¿funciona?
}
```

### Resumen: las cuatro fallas de "100% = correcto"

```
  ┌────────────────────────────────────────────────────────────────────┐
  │                                                                    │
  │  FALLA 1: EJECUCIÓN SIN VERIFICACIÓN                              │
  │  ─────────────────────────────────                                │
  │  El test ejecuta el código pero no verifica el resultado.         │
  │  100% coverage, 0% detección.                                     │
  │                                                                    │
  │  FALLA 2: VERIFICACIÓN SIN AMPLITUD                               │
  │  ──────────────────────────────                                   │
  │  El test verifica resultados pero solo para inputs "normales".    │
  │  No prueba límites, overflow, NULL, concurrencia, etc.            │
  │                                                                    │
  │  FALLA 3: VERIFICACIÓN SIN PROFUNDIDAD                            │
  │  ───────────────────────────────────                              │
  │  El test verifica ALGUNOS efectos pero no TODOS.                  │
  │  Ejemplo: verifica count pero no total_value.                     │
  │                                                                    │
  │  FALLA 4: CÓDIGO CORRECTO PARA HOY                                │
  │  ──────────────────────────────                                   │
  │  El código y tests son correctos, pero no previenen               │
  │  regresiones futuras. La cobertura mide el presente,              │
  │  no la resistencia al cambio.                                      │
  │                                                                    │
  └────────────────────────────────────────────────────────────────────┘
```

### Fórmula mental para interpretar cobertura

```
Confianza real ≈ Coverage × Calidad_de_assertions × Amplitud_de_inputs

Donde:
  Coverage: % de código ejecutado (lo que mide gcov)
  Calidad_de_assertions: ¿cada test verifica el resultado correcto?
  Amplitud_de_inputs: ¿se prueban límites, errores, edge cases?

Ejemplo:
  100% × 0% × 0% = 0%   (ejecutar sin verificar)
  50%  × 90% × 80% = 36% (pocos tests pero buenos)
  90%  × 90% × 90% = 73% (buen equilibrio)
```

---

## 11. Anatomía de un reporte: leer gcov línea por línea

### El archivo .gcov como herramienta de análisis

Un `.gcov` bien leído dice más que el porcentaje. Veamos cómo analizar uno de forma metódica.

### Código de ejemplo

```c
// parser.c — Simple parser de pares key=value
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char key[64];
    char value[256];
} KVPair;

typedef struct {
    KVPair *pairs;
    size_t  count;
    size_t  capacity;
} Config;

Config *config_create(size_t initial_cap) {
    Config *cfg = malloc(sizeof(Config));
    if (!cfg) return NULL;

    cfg->pairs = malloc(initial_cap * sizeof(KVPair));
    if (!cfg->pairs) {
        free(cfg);
        return NULL;
    }

    cfg->count = 0;
    cfg->capacity = initial_cap;
    return cfg;
}

void config_destroy(Config *cfg) {
    if (!cfg) return;
    free(cfg->pairs);
    free(cfg);
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

bool config_parse_line(Config *cfg, const char *line) {
    if (!cfg || !line) return false;

    // Skip comments and empty lines
    while (isspace((unsigned char)*line)) line++;
    if (*line == '\0' || *line == '#') return true;  // Not an error, just skip

    // Find '='
    const char *eq = strchr(line, '=');
    if (!eq) return false;  // Malformed line

    // Extract key
    size_t key_len = eq - line;
    if (key_len == 0 || key_len >= 64) return false;

    char key_buf[64];
    memcpy(key_buf, line, key_len);
    key_buf[key_len] = '\0';

    // Extract value
    const char *val_start = eq + 1;
    size_t val_len = strlen(val_start);
    if (val_len >= 256) return false;

    char val_buf[256];
    strcpy(val_buf, val_start);

    // Trim
    char *key = trim(key_buf);
    char *val = trim(val_buf);

    if (strlen(key) == 0) return false;

    // Grow if needed
    if (cfg->count >= cfg->capacity) {
        size_t new_cap = cfg->capacity * 2;
        KVPair *new_pairs = realloc(cfg->pairs, new_cap * sizeof(KVPair));
        if (!new_pairs) return false;
        cfg->pairs = new_pairs;
        cfg->capacity = new_cap;
    }

    // Store
    strcpy(cfg->pairs[cfg->count].key, key);
    strcpy(cfg->pairs[cfg->count].value, val);
    cfg->count++;
    return true;
}

const char *config_get(const Config *cfg, const char *key) {
    if (!cfg || !key) return NULL;
    for (size_t i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->pairs[i].key, key) == 0) {
            return cfg->pairs[i].value;
        }
    }
    return NULL;
}

bool config_load_file(Config *cfg, const char *path) {
    if (!cfg || !path) return false;

    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    bool ok = true;

    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (!config_parse_line(cfg, line)) {
            ok = false;
            break;
        }
    }

    fclose(f);
    return ok;
}
```

### Tests parciales

```c
// test_parser.c
#include <assert.h>
#include <string.h>
#include "parser.c"

int main(void) {
    Config *cfg = config_create(4);
    assert(cfg != NULL);

    // Parsear líneas válidas
    assert(config_parse_line(cfg, "name=Alice"));
    assert(config_parse_line(cfg, "age=30"));
    assert(config_parse_line(cfg, "  city = New York  "));

    // Verificar valores
    assert(strcmp(config_get(cfg, "name"), "Alice") == 0);
    assert(strcmp(config_get(cfg, "age"), "30") == 0);
    assert(strcmp(config_get(cfg, "city"), "New York") == 0);

    // Clave no existente
    assert(config_get(cfg, "phone") == NULL);

    // Comentario y línea vacía (deben retornar true)
    assert(config_parse_line(cfg, "# comment"));
    assert(config_parse_line(cfg, ""));
    assert(config_parse_line(cfg, "   "));

    config_destroy(cfg);
    return 0;
}
```

### Reporte gcov anotado con análisis

```bash
$ gcc -O0 -g --coverage -o test_parser test_parser.c
$ ./test_parser
$ gcov -f -b parser.c
```

```
        -:    0:Source:parser.c
        -:    0:Graph:parser.gcno
        -:    0:Data:parser.gcda
        -:    0:Runs:1                      ← Una sola ejecución
        -:    1:// parser.c
        -:    2:#include <stdio.h>
        -:    3:#include <stdlib.h>
        -:    4:#include <string.h>
        -:    5:#include <ctype.h>
```

**config_create** — 100% line, 50% branch:

```
        1:   19:Config *config_create(size_t initial_cap) {
        1:   20:    Config *cfg = malloc(sizeof(Config));
        1:   21:    if (!cfg) return NULL;              ← #1: malloc nunca falló
branch  0 taken 0%  (fallthrough)                        ← OOM nunca simulado
branch  1 taken 100%
        -:   22:
        1:   23:    cfg->pairs = malloc(initial_cap * sizeof(KVPair));
        1:   24:    if (!cfg->pairs) {                  ← #2: segundo malloc nunca falló
branch  0 taken 0%
branch  1 taken 100%
    #####:   25:        free(cfg);
    #####:   26:        return NULL;
        -:   27:    }
        -:   28:
        1:   29:    cfg->count = 0;
        1:   30:    cfg->capacity = initial_cap;
        1:   31:    return cfg;
        -:   32:}
```

**Análisis**:

```
  config_create():
  ┌─────────────────────────────────────────────────────────────┐
  │  Line coverage: 80% (8/10) — las líneas 25-26 no ejecutadas│
  │  Branch coverage: 50% (2/4) — solo la rama "éxito"         │
  │                                                             │
  │  Riesgo: Si malloc() falla en producción, el código de     │
  │  limpieza (líneas 25-26) NUNCA fue verificado.             │
  │  ¿El free(cfg) en línea 25 es correcto? Sí, pero solo lo  │
  │  sabemos por inspección visual, no por testing.            │
  │                                                             │
  │  Acción: Test con mock de malloc (--wrap=malloc) que        │
  │  simule fallo en el segundo malloc.                         │
  │  Prioridad: Media (código defensivo simple)                 │
  └─────────────────────────────────────────────────────────────┘
```

**config_parse_line** — La función más interesante:

```
        8:   44:bool config_parse_line(Config *cfg, const char *line) {
        8:   45:    if (!cfg || !line) return false;
branch  0 taken 0%                                      ← cfg=NULL nunca
branch  1 taken 100%
branch  2 taken 0%                                      ← line=NULL nunca
branch  3 taken 100%
        -:   46:
       12:   47:    while (isspace((unsigned char)*line)) line++;
branch  0 taken 33%
branch  1 taken 67%
        8:   48:    if (*line == '\0' || *line == '#') return true;
branch  0 taken 25%                                     ← vacío 2 veces
branch  1 taken 75%
branch  2 taken 17%                                     ← '#' 1 vez
branch  3 taken 83%
        -:   49:
        5:   50:    const char *eq = strchr(line, '=');
        5:   51:    if (!eq) return false;
branch  0 taken 0%                                      ← ¡NUNCA! No testeamos línea malformada
branch  1 taken 100%
        -:   52:
        5:   53:    size_t key_len = eq - line;
        5:   54:    if (key_len == 0 || key_len >= 64) return false;
branch  0 taken 0%                                      ← "=value" nunca testeado
branch  1 taken 100%
branch  2 taken 0%                                      ← key muy larga nunca testeada
branch  3 taken 100%
    #####:   55:
        -:   56:    char key_buf[64];
        5:   57:    memcpy(key_buf, line, key_len);
        5:   58:    key_buf[key_len] = '\0';
        -:   59:
        5:   60:    const char *val_start = eq + 1;
        5:   61:    size_t val_len = strlen(val_start);
        5:   62:    if (val_len >= 256) return false;
branch  0 taken 0%                                      ← valor largo nunca testeado
branch  1 taken 100%
        -:   63:
        5:   64:    char val_buf[256];
        5:   65:    strcpy(val_buf, val_start);
        -:   66:
        5:   67:    char *key = trim(key_buf);
        5:   68:    char *val = trim(val_buf);
        -:   69:
        5:   70:    if (strlen(key) == 0) return false;
branch  0 taken 0%                                      ← key vacía tras trim nunca testeada
branch  1 taken 100%
        -:   71:
        5:   72:    if (cfg->count >= cfg->capacity) {
branch  0 taken 0%                                      ← ¡NUNCA! No testeamos crecimiento
branch  1 taken 100%
    #####:   73:        size_t new_cap = cfg->capacity * 2;
    #####:   74:        KVPair *new_pairs = realloc(cfg->pairs, new_cap * sizeof(KVPair));
    #####:   75:        if (!new_pairs) return false;
    #####:   76:        cfg->pairs = new_pairs;
    #####:   77:        cfg->capacity = new_cap;
        -:   78:    }
        -:   79:
        5:   80:    strcpy(cfg->pairs[cfg->count].key, key);
        5:   81:    strcpy(cfg->pairs[cfg->count].value, val);
        5:   82:    cfg->count++;
        5:   83:    return true;
        -:   84:}
```

**Análisis detallado de config_parse_line**:

```
  ┌────────────────────────────────────────────────────────────────────────┐
  │  config_parse_line() — Análisis de cobertura                          │
  │                                                                        │
  │  Line coverage:    76.2% (16/21)                                      │
  │  Branch coverage:  50.0% (10/20)                                      │
  │                                                                        │
  │  CÓDIGO NO CUBIERTO (ordenado por riesgo):                            │
  │                                                                        │
  │  ALTO RIESGO:                                                          │
  │  1. Líneas 73-77: realloc path (crecimiento del array)               │
  │     → Si el array crece, ¿funciona? ¿Y si realloc falla?             │
  │     → Test: parsear más líneas que initial_capacity                    │
  │                                                                        │
  │  2. Línea 51: línea sin '=' (malformada)                              │
  │     → ¿Retorna false correctamente? Probablemente sí, pero no        │
  │       sabemos si el programa maneja bien el false                      │
  │     → Test: config_parse_line(cfg, "no_equals_here")                  │
  │                                                                        │
  │  MEDIO RIESGO:                                                         │
  │  3. Línea 54: key_len == 0 ("=value")                                │
  │     → Test: config_parse_line(cfg, "=value")                          │
  │                                                                        │
  │  4. Línea 54: key_len >= 64 (key demasiado larga)                    │
  │     → Test: config_parse_line(cfg, "aaaa...64chars...=val")           │
  │                                                                        │
  │  5. Línea 62: val_len >= 256 (valor demasiado largo)                  │
  │     → Test: config_parse_line(cfg, "k=" + 256 chars)                  │
  │                                                                        │
  │  BAJO RIESGO:                                                          │
  │  6. Línea 45: cfg=NULL o line=NULL                                    │
  │     → Guards defensivos, bajo riesgo pero fácil de testear            │
  │                                                                        │
  │  7. Línea 70: key vacía tras trim ("   =value")                      │
  │     → Edge case poco probable en producción                            │
  └────────────────────────────────────────────────────────────────────────┘
```

**config_load_file** — 0% (nunca llamada):

```
    #####:   90:bool config_load_file(Config *cfg, const char *path) {
    #####:   91:    if (!cfg || !path) return false;
    #####:   92:
    #####:   93:    FILE *f = fopen(path, "r");
    #####:   94:    if (!f) return false;
    #####:   95:
    #####:   96:    char line[512];
    #####:   97:    bool ok = true;
    #####:   98:
    #####:   99:    while (fgets(line, sizeof(line), f)) {
    #####:  100:        size_t len = strlen(line);
    #####:  101:        if (len > 0 && line[len - 1] == '\n') {
    #####:  102:            line[len - 1] = '\0';
    #####:  103:        }
    #####:  104:
    #####:  105:        if (!config_parse_line(cfg, line)) {
    #####:  106:            ok = false;
    #####:  107:            break;
    #####:  108:        }
    #####:  109:    }
    #####:  110:
    #####:  111:    fclose(f);
    #####:  112:    return ok;
    #####:  113:}
```

**Análisis**:

```
  config_load_file(): 0% coverage

  ┌────────────────────────────────────────────────────────────────────┐
  │  TODA la función está sin testear.                                 │
  │                                                                    │
  │  Esto es grave porque config_load_file() es probablemente la      │
  │  función que el usuario final llama. Es la interfaz de alto       │
  │  nivel, y contiene:                                                │
  │  - I/O de archivos (fopen, fgets, fclose)                         │
  │  - Manejo de newlines                                              │
  │  - Propagación de errores desde config_parse_line                  │
  │  - Cleanup correcto del FILE*                                      │
  │                                                                    │
  │  Acción: Crear archivo temporal y testear la carga completa.      │
  │  Considerar también: archivo vacío, archivo sin newline final,    │
  │  archivo con líneas muy largas (>512 bytes).                      │
  └────────────────────────────────────────────────────────────────────┘
```

---

## 12. Anatomía de un reporte: leer lcov/genhtml

### Estructura del reporte HTML

El reporte genhtml tiene tres niveles de navegación:

```
  Nivel 1: RESUMEN DEL PROYECTO (index.html)
  ┌─────────────────────────────────────────────────────────────────────┐
  │  LCOV - code coverage report                                       │
  │  Test: Config Parser          Date: 2026-04-05                     │
  │                                                                     │
  │  ┌─────────────────────────────────────────────────────────────┐   │
  │  │  Lines:      ████████████░░░░░░░░  62.5%  (40/64)          │   │
  │  │  Functions:  █████████████████░░░  80.0%  (4/5)            │   │
  │  │  Branches:   ████████░░░░░░░░░░░░  41.7%  (20/48)         │   │
  │  └─────────────────────────────────────────────────────────────┘   │
  │                                                                     │
  │  Directory              Lines     Func      Branch                  │
  │  ─────────────────────────────────────────────────                  │
  │  src/                   62.5%     80.0%     41.7%     [click]      │
  │                                                                     │
  │  Lectura: Un vistazo global al proyecto.                           │
  │  Acción: Identificar directorios con baja cobertura.               │
  └─────────────────────────────────────────────────────────────────────┘

  Nivel 2: RESUMEN POR DIRECTORIO (src/index.html)
  ┌─────────────────────────────────────────────────────────────────────┐
  │  Directory: src/                                                    │
  │                                                                     │
  │  File                   Lines     Func      Branch                  │
  │  ─────────────────────────────────────────────────                  │
  │  parser.c               62.5%     80.0%     41.7%     [click]      │
  │                                                                     │
  │  Lectura: ¿Cuáles archivos necesitan atención?                     │
  │  El color indica urgencia:                                          │
  │    Verde  (>= 90%): bien cubierto                                  │
  │    Amarillo (75-89%): aceptable, mejorable                         │
  │    Rojo   (< 75%): necesita tests                                  │
  └─────────────────────────────────────────────────────────────────────┘

  Nivel 3: DETALLE POR ARCHIVO (src/parser.c.gcov.html)
  ┌─────────────────────────────────────────────────────────────────────┐
  │  File: src/parser.c                                                 │
  │                                                                     │
  │  ┌───────────────────────────────────────────────────────────────┐ │
  │  │  Lines:  62.5%   Functions: 80.0%   Branches: 41.7%         │ │
  │  └───────────────────────────────────────────────────────────────┘ │
  │                                                                     │
  │  Line  Count   Source                                               │
  │  ──────────────────────────────────────                            │
  │   19   1       Config *config_create(size_t initial_cap) {         │
  │   20   1       ░░ Config *cfg = malloc(sizeof(Config));            │
  │   21   1       ░░ if (!cfg) return NULL;                           │
  │   ...                                                               │
  │   73   0       ██ size_t new_cap = cfg->capacity * 2;   ← ROJO    │
  │   74   0       ██ KVPair *new_pairs = realloc(...);     ← ROJO    │
  │   ...                                                               │
  │                                                                     │
  │  ░░ = ejecutado (verde)                                            │
  │  ██ = NO ejecutado (rojo)                                          │
  │     = no ejecutable (blanco)                                       │
  └─────────────────────────────────────────────────────────────────────┘
```

### Cómo leer el detalle de ramas en genhtml

En la vista por archivo, con branch coverage activado, cada línea con decisión muestra sus ramas:

```
  Line  Count  Branch              Source
  ─────────────────────────────────────────────────────────────────
  45    8      [+ - + -]           if (!cfg || !line) return false;
  │            │ │ │ │
  │            │ │ │ └─ line=NULL: no tomada (-)
  │            │ │ └─── line!=NULL: tomada (+)
  │            │ └───── cfg=NULL: no tomada (-)
  │            └─────── cfg!=NULL: tomada (+)
  │
  48    8      [+ + + +]           if (*line == '\0' || *line == '#') return true;
  │            │ │ │ │
  │            └─┴─┴─┴── Todas las ramas tomadas
  │
  51    5      [- +]               if (!eq) return false;
               │ │
               │ └── eq!=NULL: tomada (+)
               └──── eq==NULL: NUNCA tomada (-) ← Sin test para línea malformada
```

Leyenda de ramas en genhtml:

```
Símbolo     Significado
──────────────────────────────
+           Rama tomada al menos una vez
-           Rama NUNCA tomada
#           Rama en código no ejecutado (toda la función 0%)
```

### Proceso de análisis de un reporte HTML

```
  PASO 1: Mirar el resumen global
  ┌──────────────────────────────────────────────────────────┐
  │  ¿Line coverage > 70%?      → Sí: aceptable como punto │
  │                                      de partida         │
  │  ¿Function coverage = 100%? → No: hay funciones sin     │
  │                                    tests (prioridad 1)  │
  │  ¿Branch coverage > 60%?    → No: ramas de error sin    │
  │                                    tests (prioridad 2)  │
  └──────────────────────────────────────────────────────────┘

  PASO 2: Identificar archivos rojos
  ┌──────────────────────────────────────────────────────────┐
  │  Ordenar por % de líneas (menor primero).               │
  │  Los archivos rojos son candidatos a tests.             │
  │  Pero primero preguntarse:                               │
  │    ¿Es código de producción o código de debug/utils?    │
  └──────────────────────────────────────────────────────────┘

  PASO 3: Entrar al archivo más crítico
  ┌──────────────────────────────────────────────────────────┐
  │  Buscar bloques rojos (líneas 0 ejecuciones).           │
  │  Clasificar cada bloque rojo:                            │
  │    a) Manejo de errores → Prioridad alta                │
  │    b) Rama alternativa de lógica → Prioridad alta       │
  │    c) Código defensivo (NULL checks) → Prioridad media  │
  │    d) Código de debug/logging → Prioridad baja          │
  └──────────────────────────────────────────────────────────┘

  PASO 4: Verificar ramas
  ┌──────────────────────────────────────────────────────────┐
  │  Buscar [-] en las anotaciones de ramas.                │
  │  Cada [-] es una rama que no se testeó.                 │
  │  ¿Es una rama de error? → Test con ese error            │
  │  ¿Es una rama de lógica? → Test con esa condición       │
  └──────────────────────────────────────────────────────────┘

  PASO 5: Priorizar
  ┌──────────────────────────────────────────────────────────┐
  │  1. Funciones 0% que son API pública                    │
  │  2. Ramas de error en funciones de I/O                  │
  │  3. Ramas de lógica en funciones complejas              │
  │  4. Código defensivo (NULL guards)                      │
  │  5. Código de debug/dump/print                          │
  └──────────────────────────────────────────────────────────┘
```

---

## 13. Patrones de código problemático para cobertura

### Patrón 1: Código defensivo difícil de alcanzar

```c
void *safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "Out of memory\n");  // ← ¿Cómo testear esto?
        abort();
    }
    return ptr;
}
```

El `if (!ptr)` es código defensivo importante pero difícil de testear sin mocking. Opciones:

```
Opción                     Esfuerzo    Cubre la rama?
──────────────────────────────────────────────────────
Ignorar (aceptar 0%)      Ninguno     No
Mock con --wrap=malloc     Medio       Sí
Injection via fnptr        Alto        Sí
Marcar como excluida       Ninguno     N/A (se excluye)
```

### Patrón 2: Switch con muchos cases

```c
const char *http_status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default:  return "Unknown";
    }
}
```

17 ramas. Para 100% branch coverage necesitas 17 tests (uno por status code). ¿Vale la pena?

```
Análisis:
  - La función es trivial (lookup table glorificada)
  - Cada rama es idéntica en estructura (return string)
  - El riesgo de bug es mínimo (typo en string literal)
  - El esfuerzo para 100% es alto (17 tests repetitivos)

Decisión:
  → Testear 3-4 representativos + el default
  → Aceptar ~30% branch coverage en esta función
  → Documentar la decisión: "http_status_text: lookup table,
    cobertura parcial intencional"
```

### Patrón 3: Manejo de errores en cascada

```c
bool init_system(void) {
    if (!init_logging()) {
        return false;                           // Rama error 1
    }
    if (!init_database()) {
        shutdown_logging();
        return false;                           // Rama error 2
    }
    if (!init_cache()) {
        shutdown_database();
        shutdown_logging();
        return false;                           // Rama error 3
    }
    if (!init_network()) {
        shutdown_cache();
        shutdown_database();
        shutdown_logging();
        return false;                           // Rama error 4
    }
    return true;
}
```

Para branch coverage completo necesitas 5 tests, cada uno forzando un fallo diferente. Y cada path de error tiene su propia secuencia de cleanup que debe verificarse.

```
  Prioridad de testing:

  ALTA:
  - ¿El cleanup en la rama 4 llama a los 3 shutdowns?
  - ¿En el orden correcto?
  - ¿Y si shutdown_database() falla durante cleanup?

  El reporte de branch coverage diría "rama 4 no cubierta"
  pero la pregunta real es "¿el cleanup es correcto?"
```

### Patrón 4: Logging y debug code

```c
void process_order(Order *order) {
    LOG_DEBUG("Processing order %d", order->id);    // ← ¿Testear esto?

    if (validate_order(order)) {
        LOG_INFO("Order %d validated", order->id);  // ← ¿Y esto?
        execute_order(order);
    } else {
        LOG_WARN("Order %d invalid: %s",            // ← ¿Y esto?
                 order->id, get_last_error());
    }
}
```

El código de logging es ejecutable y gcov lo cuenta. Si LOG_DEBUG se compila como no-op en test mode, las líneas son "no ejecutables". Si se compila como real, contribuyen a la cobertura sin ser lo que quieres testear.

### Patrón 5: Código generado o boilerplate

```c
// Getters y setters generados por macro
#define GETTER(type, name) \
    type get_##name(const Config *cfg) { \
        if (!cfg) return (type){0}; \
        return cfg->name; \
    }

GETTER(int, width)
GETTER(int, height)
GETTER(const char *, title)
GETTER(bool, fullscreen)
GETTER(double, fps)
// ... 20 más
```

Cada getter genera ~3 líneas ejecutables. 25 getters = 75 líneas. Para 100% necesitas 50 tests (normal + NULL para cada getter). ¿Vale la pena?

### Estrategia para código problemático

```
Tipo de código                 Estrategia recomendada
──────────────────────────────────────────────────────────────────────
Malloc failure                 Mock con --wrap si es crítico; aceptar
                               baja cobertura si es abort()
Switch lookup table            3-4 tests representativos + default
Error cascade (init/cleanup)   Testear cada cascade con DI/mocking
Logging                        Excluir del reporte o aceptar
Getters/setters triviales      1-2 representativos, aceptar el resto
Platform-specific (#ifdef)     Testear en cada plataforma o excluir
Código inalcanzable            Eliminar si es realmente inalcanzable
```

---

## 14. Cobertura y calidad de tests: la distinción crítica

### Las tres dimensiones de un buen test

```
  Dimensión 1: COBERTURA (qué código se ejecuta)
  ┌────────────────────────────────────────────────────────────────┐
  │  Medible con gcov/lcov                                         │
  │  Responde: ¿Se ejecutó el código?                              │
  │  NO responde: ¿El resultado es correcto?                       │
  └────────────────────────────────────────────────────────────────┘

  Dimensión 2: ASSERTIONS (qué se verifica)
  ┌────────────────────────────────────────────────────────────────┐
  │  Medible con: mutation testing, inspection manual              │
  │  Responde: ¿El test detectaría un bug?                         │
  │  Ejemplo: assert(resultado == esperado)                        │
  └────────────────────────────────────────────────────────────────┘

  Dimensión 3: INPUTS (qué escenarios se prueban)
  ┌────────────────────────────────────────────────────────────────┐
  │  Medible con: análisis de particiones, boundary analysis       │
  │  Responde: ¿Se probaron los casos importantes?                │
  │  Ejemplo: NULL, vacío, máximo, negativo, unicode, etc.        │
  └────────────────────────────────────────────────────────────────┘
```

### Mutation testing: medir la calidad real

**Mutation testing** introduce cambios pequeños (mutaciones) en el código y verifica que al menos un test falle. Si ningún test falla, el mutante "sobrevivió" — el test suite es débil en esa zona.

```c
// Código original
int max(int a, int b) {
    return (a > b) ? a : b;
}

// Mutante 1: cambiar > por >=
int max(int a, int b) {
    return (a >= b) ? a : b;    // ¿Algún test falla?
}

// Mutante 2: cambiar > por <
int max(int a, int b) {
    return (a < b) ? a : b;     // ¿Algún test falla?
}

// Mutante 3: retornar siempre a
int max(int a, int b) {
    return a;                    // ¿Algún test falla?
}

// Mutante 4: retornar siempre b
int max(int a, int b) {
    return b;                    // ¿Algún test falla?
}
```

```
Test: assert(max(3, 1) == 3)

  Mutante 1 (>=): max(3,1) → 3 ✓ → SOBREVIVE (test débil para a==b)
  Mutante 2 (<):  max(3,1) → 1 ✗ → MUERTO ✓
  Mutante 3 (=a): max(3,1) → 3 ✓ → SOBREVIVE (test débil)
  Mutante 4 (=b): max(3,1) → 1 ✗ → MUERTO ✓

  Mutation score: 2/4 = 50% (necesitamos test con a < b y a == b)
```

Añadiendo: `assert(max(1, 3) == 3)` y `assert(max(5, 5) == 5)`:

```
  Mutante 1 (>=): max(5,5) → 5 ✓ → SOBREVIVE (>= y > dan mismo resultado con ==)
  Mutante 2 (<):  max(1,3) → 1 ✗ → MUERTO ✓
  Mutante 3 (=a): max(1,3) → 1 ✗ → MUERTO ✓
  Mutante 4 (=b): max(3,1) → 1 ✗ → MUERTO ✓

  Mutation score: 3/4 = 75% (mutante 1 es equivalente — no puede matarse)
```

### Herramientas de mutation testing en C

```
Herramienta       Estado          Descripción
──────────────────────────────────────────────────────────
mull              Activa          LLVM-based, buena calidad
dextool mutate    Activa          D-based, soporta C
mutmut            Python only     No aplica a C
pitest            Java only       No aplica a C
CCMutator         Académico       Research prototype
```

### Relación entre cobertura y mutation score

```
                        Mutation Score
                    Alto            Bajo
                ──────────────  ──────────────
  Coverage      │              │              │
  Alto          │  IDEAL       │  TESTS       │
                │  Tests       │  VACÍOS      │
                │  robustos    │  (ejecutan   │
                │              │  sin assert) │
                ├──────────────┼──────────────┤
  Bajo          │  IMPROBABLE  │  NO HAY      │
                │  (pero       │  TESTS       │
                │  posible con │              │
                │  tests       │              │
                │  focalizados)│              │
                └──────────────┴──────────────┘
```

---

## 15. Umbrales de cobertura: cuánto es suficiente

### La pregunta incorrecta vs la correcta

```
Pregunta incorrecta:  "¿Cuál es el % de cobertura ideal?"
Pregunta correcta:    "¿Qué código no cubierto me pone en riesgo?"
```

### Umbrales por tipo de proyecto

```
Tipo de proyecto              Line    Branch   Function   Justificación
──────────────────────────────────────────────────────────────────────────────
Safety-critical (DO-178C A)    100%    MC/DC    100%      Regulación
Medical device (IEC 62304 C)   100%    100%     100%      Regulación
Automotive (ISO 26262 ASIL D)  100%    MC/DC    100%      Regulación
Infraestructura financiera     90%+    80%+     100%      Riesgo monetario
Biblioteca open source         80%+    70%+     95%+      Confianza usuarios
Aplicación de producción       70%+    60%+     90%+      Equilibrio costo/riesgo
Microservicio interno          60%+    50%+     85%+      Iteración rápida
Prototipo / MVP                40%+    -        70%+      Exploración
Script interno de una vez      -       -        -         No justifica inversión
```

### Rendimientos decrecientes

```
  Esfuerzo para alcanzar cobertura:

  Cobertura │
  100% ─────┤                                           ╱
            │                                         ╱
   90% ─────┤                                    ╱──╱
            │                               ╱──╱
   80% ─────┤                          ╱──╱
            │                    ╱───╱
   70% ─────┤              ╱──╱
            │         ╱──╱
   60% ─────┤    ╱──╱
            │ ╱╱
   50% ─────┤╱
            │
    0% ─────┼──────┬──────┬──────┬──────┬──────┬──────
            0      1      2      3      4      5
                          Esfuerzo (relativo)

  Interpretación:
  - De 0% a 60%: bajo esfuerzo, alto retorno
  - De 60% a 80%: esfuerzo moderado, buen retorno
  - De 80% a 90%: esfuerzo alto, retorno moderado
  - De 90% a 95%: esfuerzo muy alto, retorno bajo
  - De 95% a 100%: esfuerzo extremo, retorno casi nulo
                    (mocking de malloc, testing de abort paths,
                     plataformas alternativas, etc.)
```

### La regla del 80/20 aplicada a cobertura

```
  El 80% de los bugs se encuentran testeando el 20% del código más complejo.

  Estrategia:
  1. Identificar las funciones más complejas (más ramas, más lógica)
  2. Testear esas funciones exhaustivamente (branch coverage ~100%)
  3. Aceptar cobertura menor en código trivial (getters, logging, etc.)
  4. Resultado: 75% cobertura global pero 95% en código crítico
```

### Umbrales como ratchet (trinquete)

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  CONCEPTO: Coverage Ratchet                                     ║
 ║                                                                  ║
 ║  En vez de fijar un umbral absoluto ("siempre >= 80%"),         ║
 ║  usar un trinquete: "nunca bajar del máximo alcanzado".         ║
 ║                                                                  ║
 ║  Semana 1: 45% → nuevo umbral = 45%                            ║
 ║  Semana 2: 52% → nuevo umbral = 52%                            ║
 ║  Semana 3: 50% → CI FALLA (bajó de 52%)                        ║
 ║  Semana 4: 55% → nuevo umbral = 55%                            ║
 ║                                                                  ║
 ║  Esto garantiza mejora monotónica sin exigir saltos grandes.    ║
 ╚══════════════════════════════════════════════════════════════════╝
```

Implementación con script:

```bash
#!/bin/bash
# coverage_ratchet.sh

THRESHOLD_FILE=".coverage_threshold"
CURRENT=$(lcov --summary filtered.info 2>&1 | \
    grep 'lines' | grep -oP '[\d.]+(?=%)')

if [ -f "$THRESHOLD_FILE" ]; then
    THRESHOLD=$(cat "$THRESHOLD_FILE")
else
    THRESHOLD="0"
fi

echo "Current: ${CURRENT}%  Threshold: ${THRESHOLD}%"

if [ "$(echo "$CURRENT < $THRESHOLD" | bc -l)" -eq 1 ]; then
    echo "FAIL: Coverage dropped from ${THRESHOLD}% to ${CURRENT}%"
    exit 1
fi

# Actualizar umbral si subió
if [ "$(echo "$CURRENT > $THRESHOLD" | bc -l)" -eq 1 ]; then
    echo "$CURRENT" > "$THRESHOLD_FILE"
    echo "Threshold updated to ${CURRENT}%"
fi

echo "PASS"
```

---

## 16. Anti-patrones: gaming the coverage

### Anti-patrón 1: tests sin assertions

```c
// MALO: test que ejecuta pero no verifica
void test_sort(void) {
    int arr[] = {5, 3, 1, 4, 2};
    bubble_sort(arr, 5);
    // ¡Sin assert! Solo ejecuta para subir el %
}
```

**Detección**: mutation testing mata estos tests fácilmente. Si ningún mutante muere, el test no verifica nada.

### Anti-patrón 2: tests que repiten el código

```c
// MALO: el test reimplementa la lógica en vez de verificar resultados conocidos
void test_tax(void) {
    double income = 50000;
    double expected;

    // Reimplementa la misma lógica que calcula el tax
    if (income < 10000) expected = 0;
    else if (income < 40000) expected = (income - 10000) * 0.2;
    else expected = 6000 + (income - 40000) * 0.4;

    assert(calculate_tax(income) == expected);
    // Si hay un bug en la lógica, ¡el test tiene el mismo bug!
}

// BUENO: usar valores precalculados manualmente o de una fuente externa
void test_tax(void) {
    assert(calculate_tax(5000) == 0.0);
    assert(calculate_tax(20000) == 2000.0);
    assert(calculate_tax(50000) == 10000.0);
}
```

### Anti-patrón 3: excluir todo lo difícil

```bash
# MALO: excluir tanto que el % es artificialmente alto
lcov --remove coverage.info \
    '/usr/*' \
    '*/error_*' \
    '*/init_*' \
    '*/cleanup_*' \
    '*/validate_*' \
    -o filtered.info

# Resultado: 95% coverage... porque excluimos el código complejo
```

### Anti-patrón 4: tests que dependen de orden de ejecución

```c
static int global_counter = 0;

void test_increment(void) {
    global_counter++;
    assert(global_counter == 1);  // Solo pasa si es el primer test
}

void test_check(void) {
    assert(global_counter == 1);  // Solo pasa si test_increment corrió primero
}
```

### Anti-patrón 5: test con constantes mágicas

```c
// MALO: ¿de dónde sale 42? ¿Es correcto?
void test_hash(void) {
    assert(hash("hello") == 0x4F9F2CAB);
    // ¿Se calculó este hash manualmente?
    // ¿O se copió de la salida de la primera ejecución?
    // Si el hash estaba mal desde el inicio, este test "pasa" con el bug
}
```

### Cómo detectar gaming

```
Señal de alerta                    Indicador
──────────────────────────────────────────────────────────────────────
Line coverage alta, branch baja    Tests solo cubren happy path
Line coverage alta, sin asserts    grep 'assert' tests/ | wc -l bajo
Coverage subió 20% en 1 commit     Probablemente tests vacíos
Mutation score bajo con cov alta   Tests ejecutan pero no verifican
Muchos tests, pocos assert/test    Ratio assert/test < 1
Todos los tests pasan siempre      Nadie introdujo un test que falle
```

---

## 17. Cobertura como herramienta de diseño

### Cobertura como feedback sobre la arquitectura

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  Si una función es difícil de testear, probablemente            ║
 ║  tiene un problema de diseño.                                    ║
 ║                                                                  ║
 ║  Testabilidad y buen diseño están correlacionados.              ║
 ╚══════════════════════════════════════════════════════════════════╝
```

### Señal 1: Función con demasiadas ramas

```c
// Difícil de testear: 12 ramas, múltiples responsabilidades
int process_request(Request *req) {
    if (!req) return ERR_NULL;
    if (!req->headers) return ERR_NO_HEADERS;
    if (!validate_auth(req)) return ERR_AUTH;
    if (!validate_content_type(req)) return ERR_CONTENT_TYPE;

    switch (req->method) {
        case GET:
            if (req->has_body) return ERR_BODY_NOT_ALLOWED;
            return handle_get(req);
        case POST:
            if (!req->body) return ERR_NO_BODY;
            if (req->body_len > MAX_BODY) return ERR_TOO_LARGE;
            return handle_post(req);
        case DELETE:
            if (!check_permissions(req)) return ERR_FORBIDDEN;
            return handle_delete(req);
        default:
            return ERR_METHOD;
    }
}
// Branch coverage total: 24+ ramas
// Necesitas ~15 tests para cubrir todas las ramas
```

**Refactor guiado por la dificultad de cobertura**:

```c
// Separar validación de dispatch
static int validate_request(const Request *req) {
    if (!req) return ERR_NULL;
    if (!req->headers) return ERR_NO_HEADERS;
    if (!validate_auth(req)) return ERR_AUTH;
    if (!validate_content_type(req)) return ERR_CONTENT_TYPE;
    return 0;
}

static int validate_get(const Request *req) {
    if (req->has_body) return ERR_BODY_NOT_ALLOWED;
    return 0;
}

static int validate_post(const Request *req) {
    if (!req->body) return ERR_NO_BODY;
    if (req->body_len > MAX_BODY) return ERR_TOO_LARGE;
    return 0;
}

int process_request(Request *req) {
    int err = validate_request(req);
    if (err) return err;

    switch (req->method) {
        case GET:    err = validate_get(req);  break;
        case POST:   err = validate_post(req); break;
        case DELETE:
            if (!check_permissions(req)) return ERR_FORBIDDEN;
            break;
        default:     return ERR_METHOD;
    }
    if (err) return err;

    switch (req->method) {
        case GET:    return handle_get(req);
        case POST:   return handle_post(req);
        case DELETE:  return handle_delete(req);
        default:      return ERR_METHOD; // unreachable
    }
}

// Ahora cada función tiene 4-6 ramas, no 24
// Cada una se testea independientemente
// La cobertura de cada función es fácil de alcanzar
```

### Señal 2: Baja cobertura por dependencias externas

Si `config_load_file()` tiene 0% porque depende de I/O real, eso indica acoplamiento directo a I/O. El refactor es inyectar la dependencia:

```c
// Antes: acoplada a FILE* — difícil de testear
bool config_load_file(Config *cfg, const char *path);

// Después: acepta cualquier fuente de líneas
typedef const char *(*LineReader)(void *ctx);

bool config_load(Config *cfg, LineReader reader, void *ctx);

// Implementación con FILE*
const char *file_line_reader(void *ctx) { /* fgets from FILE* */ }

// Test con datos en memoria
const char *test_lines[] = {"key1=val1", "key2=val2", NULL};
int test_idx = 0;
const char *mock_reader(void *ctx) {
    (void)ctx;
    return test_lines[test_idx++];
}

void test_config_load(void) {
    Config *cfg = config_create(4);
    test_idx = 0;
    assert(config_load(cfg, mock_reader, NULL));
    assert(cfg->count == 2);
    config_destroy(cfg);
}
```

### Señal 3: Código no cubierto que no se puede eliminar

Si hay código que nunca se ejecuta en tests y nunca se ejecuta en producción → es código muerto. Elimínalo.

```bash
# Buscar funciones con 0% en gcov
gcov -f -n *.c 2>&1 | grep -B1 "0.00%"

# Si la función no se llama en ningún lugar:
grep -rn "unused_function" src/ tests/ main.c
# Si no hay resultados → borrar la función
```

---

## 18. Comparación de interpretación: C vs Rust vs Go

### Herramientas y métricas disponibles

```
Aspecto              C (gcov/lcov)              Rust                        Go
──────────────────────────────────────────────────────────────────────────────────────
Métricas base        Line, Function, Branch     Line, Region, Branch        Statement (block)
                                                (con llvm-cov)

Branch coverage      gcov -b (explícito)        llvm-cov: sí               No nativo
                                                tarpaulin: parcial          (go tool cover solo
                                                                            mide statements)

Interpretación       Manual (leer .gcov)        cargo-tarpaulin muestra    go tool cover -func
                     o HTML (genhtml)           % por función               (resumen por función)
                                                cargo-llvm-cov show        go tool cover -html
                                                (vista anotada)             (vista anotada)

Mutation testing     mull (LLVM)                cargo-mutants              go-mutesting
                                                (activo, bueno)            (experimental)

Exclusión            lcov --remove              #[cfg(not(tarpaulin))]     //go:nocover (no existe)
                     // LCOV_EXCL_LINE          #[coverage(off)]           build tags
                     // LCOV_EXCL_START/STOP    (nightly)

Granularidad de      Línea                      Región (sub-expresión      Bloque (statement)
reporte                                         con llvm-cov)

MC/DC                Herramientas comerciales   No nativo                  No nativo

Facilidad de         Compleja (múltiples        Simple (cargo tarpaulin)   Simple (go test -cover)
interpretación       herramientas, flags)
```

### Cómo interpretar cobertura en cada lenguaje

**C**: debes conocer gcov, lcov, branch flags, y leer .gcov manualmente o navegar HTML. La interpretación requiere experiencia porque el lenguaje no tiene safety nets (no NULL safety, no bounds checking). Branch coverage es especialmente importante porque las ramas de error protegen contra crashes.

**Rust**: el compilador ya previene muchos bugs (ownership, Option/Result). La cobertura es menos crítica porque el type system cubre parte del trabajo. Pero aún es útil para verificar lógica de negocio y edge cases.

**Go**: solo statement coverage por defecto. Go enfatiza testing robusto sobre métricas de cobertura. `go test -cover` es simple pero no mide ramas. La filosofía es "write good tests" más que "hit coverage targets".

### Exclusión de líneas en gcov/lcov

lcov soporta marcadores especiales en el código fuente:

```c
// Excluir una línea
void *ptr = malloc(size);
if (!ptr) abort();  // LCOV_EXCL_LINE

// Excluir un bloque
// LCOV_EXCL_START
void debug_dump(void) {
    // Código de debug que no necesita tests
    printf("Internal state: ...\n");
}
// LCOV_EXCL_STOP

// Excluir rama
if (!ptr) {  // LCOV_EXCL_BR_LINE
    abort();
}
```

```
 ╔══════════════════════════════════════════════════════════════════╗
 ║  ADVERTENCIA: Usar LCOV_EXCL con moderación.                   ║
 ║                                                                  ║
 ║  Cada exclusión es una decisión de "este código no necesita     ║
 ║  tests". Documenta POR QUÉ se excluye.                         ║
 ║                                                                  ║
 ║  Abuso: Excluir todo lo difícil para inflar el %.               ║
 ║  Uso legítimo: Excluir abort(), debug dumps, código generado.   ║
 ╚══════════════════════════════════════════════════════════════════╝
```

---

## 19. Ejemplo completo: auditoría de cobertura de un parser

### El escenario

Tienes un parser de expresiones aritméticas simples. Los tests fueron escritos por otro desarrollador. Tu tarea: **auditar la cobertura** e identificar qué falta.

### El código

```c
// expr_parser.h
#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

typedef enum {
    EXPR_OK = 0,
    EXPR_ERR_NULL,
    EXPR_ERR_EMPTY,
    EXPR_ERR_SYNTAX,
    EXPR_ERR_DIV_ZERO,
    EXPR_ERR_OVERFLOW,
    EXPR_ERR_PAREN
} ExprError;

// Evaluar una expresión aritmética simple.
// Soporta: +, -, *, /, paréntesis, enteros y decimales.
// Ejemplo: "3 + 4 * (2 - 1)" → 7.0
// Retorna error code, resultado en *result.
ExprError expr_eval(const char *input, double *result);

// Obtener mensaje de error legible.
const char *expr_error_string(ExprError err);

#endif
```

```c
// expr_parser.c
#include "expr_parser.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

typedef struct {
    const char *input;
    size_t      pos;
    ExprError   error;
} Parser;

static void skip_whitespace(Parser *p) {
    while (p->input[p->pos] && isspace((unsigned char)p->input[p->pos])) {
        p->pos++;
    }
}

static double parse_number(Parser *p) {
    const char *start = &p->input[p->pos];
    char *end;
    double val = strtod(start, &end);

    if (end == start) {
        p->error = EXPR_ERR_SYNTAX;
        return 0.0;
    }

    if (val > DBL_MAX || val < -DBL_MAX) {
        p->error = EXPR_ERR_OVERFLOW;
        return 0.0;
    }

    p->pos += (end - start);
    return val;
}

// Forward declarations for recursive descent
static double parse_expr(Parser *p);
static double parse_term(Parser *p);
static double parse_factor(Parser *p);

static double parse_factor(Parser *p) {
    skip_whitespace(p);

    if (p->error != EXPR_OK) return 0.0;

    // Unary minus
    if (p->input[p->pos] == '-') {
        p->pos++;
        double val = parse_factor(p);
        return -val;
    }

    // Parenthesized expression
    if (p->input[p->pos] == '(') {
        p->pos++;  // skip '('
        double val = parse_expr(p);
        skip_whitespace(p);

        if (p->error != EXPR_OK) return 0.0;

        if (p->input[p->pos] != ')') {
            p->error = EXPR_ERR_PAREN;
            return 0.0;
        }
        p->pos++;  // skip ')'
        return val;
    }

    // Number
    if (isdigit((unsigned char)p->input[p->pos]) || p->input[p->pos] == '.') {
        return parse_number(p);
    }

    p->error = EXPR_ERR_SYNTAX;
    return 0.0;
}

static double parse_term(Parser *p) {
    double left = parse_factor(p);

    while (p->error == EXPR_OK) {
        skip_whitespace(p);
        char op = p->input[p->pos];

        if (op != '*' && op != '/') break;
        p->pos++;

        double right = parse_factor(p);

        if (op == '*') {
            left *= right;
        } else {
            if (right == 0.0) {
                p->error = EXPR_ERR_DIV_ZERO;
                return 0.0;
            }
            left /= right;
        }
    }
    return left;
}

static double parse_expr(Parser *p) {
    double left = parse_term(p);

    while (p->error == EXPR_OK) {
        skip_whitespace(p);
        char op = p->input[p->pos];

        if (op != '+' && op != '-') break;
        p->pos++;

        double right = parse_term(p);

        if (op == '+') {
            left += right;
        } else {
            left -= right;
        }
    }
    return left;
}

ExprError expr_eval(const char *input, double *result) {
    if (!input || !result) return EXPR_ERR_NULL;

    // Skip leading whitespace
    while (*input && isspace((unsigned char)*input)) input++;
    if (*input == '\0') return EXPR_ERR_EMPTY;

    Parser p = {.input = input, .pos = 0, .error = EXPR_OK};
    *result = parse_expr(&p);

    if (p.error != EXPR_OK) return p.error;

    // Check for trailing garbage
    skip_whitespace(&p);
    if (p.input[p.pos] != '\0') {
        return EXPR_ERR_SYNTAX;
    }

    return EXPR_OK;
}

const char *expr_error_string(ExprError err) {
    switch (err) {
        case EXPR_OK:           return "Success";
        case EXPR_ERR_NULL:     return "NULL input or result";
        case EXPR_ERR_EMPTY:    return "Empty expression";
        case EXPR_ERR_SYNTAX:   return "Syntax error";
        case EXPR_ERR_DIV_ZERO: return "Division by zero";
        case EXPR_ERR_OVERFLOW: return "Numeric overflow";
        case EXPR_ERR_PAREN:    return "Mismatched parentheses";
        default:                return "Unknown error";
    }
}
```

### Tests existentes (escritos por otro dev)

```c
// test_expr.c
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "expr_parser.h"

#define ASSERT_EXPR(input, expected) do { \
    double _r; \
    assert(expr_eval(input, &_r) == EXPR_OK); \
    assert(fabs(_r - (expected)) < 1e-9); \
} while(0)

#define ASSERT_ERR(input, err_code) do { \
    double _r; \
    assert(expr_eval(input, &_r) == err_code); \
} while(0)

int main(void) {
    // Basic arithmetic
    ASSERT_EXPR("42", 42.0);
    ASSERT_EXPR("3 + 4", 7.0);
    ASSERT_EXPR("10 - 3", 7.0);
    ASSERT_EXPR("6 * 7", 42.0);
    ASSERT_EXPR("15 / 3", 5.0);

    // Operator precedence
    ASSERT_EXPR("2 + 3 * 4", 14.0);
    ASSERT_EXPR("10 - 2 * 3", 4.0);

    // Parentheses
    ASSERT_EXPR("(2 + 3) * 4", 20.0);
    ASSERT_EXPR("(10 - 2) * (3 + 1)", 32.0);

    // Decimals
    ASSERT_EXPR("3.14", 3.14);
    ASSERT_EXPR("1.5 + 2.5", 4.0);

    // Errors
    ASSERT_ERR(NULL, EXPR_ERR_NULL);
    ASSERT_ERR("", EXPR_ERR_EMPTY);
    ASSERT_ERR("10 / 0", EXPR_ERR_DIV_ZERO);

    printf("All tests passed\n");
    return 0;
}
```

### Generar reporte de cobertura

```bash
$ gcc -O0 -g --coverage -o test_expr test_expr.c expr_parser.c
$ ./test_expr
All tests passed

$ gcov -f -b expr_parser.c
Function 'skip_whitespace'
Lines executed:100.00% of 3

Function 'parse_number'
Lines executed:71.43% of 7

Function 'parse_factor'
Lines executed:73.33% of 15

Function 'parse_term'
Lines executed:100.00% of 12

Function 'parse_expr'
Lines executed:100.00% of 12

Function 'expr_eval'
Lines executed:80.00% of 10

Function 'expr_error_string'
Lines executed:0.00% of 9

File 'expr_parser.c'
Lines executed:76.47% of 68
Branches executed:76.19% of 42
Taken at least once:59.52% of 42
```

### Auditoría detallada

```
  ┌──────────────────────────────────────────────────────────────────────────┐
  │            AUDITORÍA DE COBERTURA — expr_parser.c                       │
  │                                                                          │
  │  Resumen: 76.5% líneas, 59.5% ramas, 85.7% funciones                  │
  ├──────────────────────────────────────────────────────────────────────────┤
  │                                                                          │
  │  1. expr_error_string(): 0%                                             │
  │     ┌────────────────────────────────────────────────────────────────┐  │
  │     │  Riesgo: BAJO — es una lookup table de strings                │  │
  │     │  Acción: Testear al menos 2-3 casos + default                 │  │
  │     │  Prioridad: Baja (pero fácil, 2 minutos)                     │  │
  │     └────────────────────────────────────────────────────────────────┘  │
  │                                                                          │
  │  2. parse_number(): 71.4% — ramas no cubiertas                         │
  │     ┌────────────────────────────────────────────────────────────────┐  │
  │     │  No cubierto:                                                  │  │
  │     │  - EXPR_ERR_SYNTAX (end == start): input que no es número     │  │
  │     │  - EXPR_ERR_OVERFLOW: número > DBL_MAX                        │  │
  │     │                                                                │  │
  │     │  Riesgo: MEDIO — syntax error no testeado                     │  │
  │     │  Test: "abc" (syntax), "1e999" (overflow)                     │  │
  │     │  Prioridad: Media                                              │  │
  │     └────────────────────────────────────────────────────────────────┘  │
  │                                                                          │
  │  3. parse_factor(): 73.3% — varias ramas críticas                      │
  │     ┌────────────────────────────────────────────────────────────────┐  │
  │     │  No cubierto:                                                  │  │
  │     │  - Unary minus: "-5", "-(3+2)"                                │  │
  │     │  - EXPR_ERR_PAREN: paréntesis sin cerrar "(3+2"              │  │
  │     │  - Early return por error propagado                            │  │
  │     │  - Syntax error por carácter inválido: "abc"                  │  │
  │     │                                                                │  │
  │     │  Riesgo: ALTO — unary minus es funcionalidad real             │  │
  │     │  Tests faltantes:                                              │  │
  │     │    ASSERT_EXPR("-5", -5.0)                                    │  │
  │     │    ASSERT_EXPR("-(3+2)", -5.0)                                │  │
  │     │    ASSERT_ERR("(3+2", EXPR_ERR_PAREN)                        │  │
  │     │    ASSERT_ERR("abc", EXPR_ERR_SYNTAX)                         │  │
  │     │  Prioridad: Alta                                               │  │
  │     └────────────────────────────────────────────────────────────────┘  │
  │                                                                          │
  │  4. expr_eval(): 80% — trailing garbage no testeado                     │
  │     ┌────────────────────────────────────────────────────────────────┐  │
  │     │  No cubierto:                                                  │  │
  │     │  - Trailing garbage check: "3 + 4 xyz" → EXPR_ERR_SYNTAX     │  │
  │     │  - result=NULL check                                           │  │
  │     │                                                                │  │
  │     │  Riesgo: MEDIO — trailing garbage es input real               │  │
  │     │  Tests faltantes:                                              │  │
  │     │    ASSERT_ERR("3 + 4 xyz", EXPR_ERR_SYNTAX)                  │  │
  │     │    assert(expr_eval("3", NULL) == EXPR_ERR_NULL)              │  │
  │     │  Prioridad: Media                                              │  │
  │     └────────────────────────────────────────────────────────────────┘  │
  │                                                                          │
  │  5. Tests existentes: calidad                                           │
  │     ┌────────────────────────────────────────────────────────────────┐  │
  │     │  ✓ Verifican resultado (no solo ejecutan)                     │  │
  │     │  ✓ Cubren los 4 operadores                                    │  │
  │     │  ✓ Cubren precedencia de operadores                           │  │
  │     │  ✓ Cubren paréntesis (caso correcto)                          │  │
  │     │  ✓ Cubren decimales                                           │  │
  │     │  ✓ Cubren NULL y empty                                        │  │
  │     │  ✓ Cubren division by zero                                    │  │
  │     │  ✗ NO cubren: unary minus, overflow, paren error,            │  │
  │     │    trailing garbage, nested parens, whitespace-only,          │  │
  │     │    expr_error_string, result=NULL                             │  │
  │     │  Calidad general: Buena base, pero incompleta                 │  │
  │     └────────────────────────────────────────────────────────────────┘  │
  │                                                                          │
  ├──────────────────────────────────────────────────────────────────────────┤
  │  PLAN DE MEJORA (ordenado por impacto/esfuerzo)                         │
  │                                                                          │
  │  1. [5 min]  Unary minus: "-5", "-3+2", "-(1+2)"                      │
  │  2. [2 min]  Paren error: "(3+2", "((3+2)"                            │
  │  3. [2 min]  Syntax error: "abc", "3 + abc", "+ 3"                     │
  │  4. [2 min]  Trailing garbage: "3 + 4 xyz", "5)"                       │
  │  5. [2 min]  expr_error_string: 3-4 representativos + default          │
  │  6. [2 min]  Edge: "   42   " (solo whitespace), result=NULL           │
  │  7. [3 min]  Overflow: "1e999"                                          │
  │  8. [2 min]  Nested: "((((1+2))))", "1+(2*(3+4))"                     │
  │                                                                          │
  │  Estimado después de mejoras: ~92% líneas, ~85% ramas                  │
  └──────────────────────────────────────────────────────────────────────────┘
```

### Mapa visual antes y después

```
  ANTES (tests existentes):

  skip_whitespace   ████████████████████  100%
  parse_number      ██████████████░░░░░░   71%   ← syntax, overflow
  parse_factor      ██████████████░░░░░░   73%   ← unary, paren, syntax
  parse_term        ████████████████████  100%
  parse_expr        ████████████████████  100%
  expr_eval         ████████████████░░░░   80%   ← trailing, result=NULL
  expr_error_string ░░░░░░░░░░░░░░░░░░░░    0%   ← sin tests


  DESPUÉS (con tests de mejora):

  skip_whitespace   ████████████████████  100%
  parse_number      ████████████████████  100%   ✓ syntax + overflow
  parse_factor      ████████████████████  100%   ✓ unary + paren + syntax
  parse_term        ████████████████████  100%
  parse_expr        ████████████████████  100%
  expr_eval         ████████████████████  100%   ✓ trailing + NULL
  expr_error_string ███████████████████░   89%   ~ 7 de 8 cases testeados
```

---

## 20. Programa de práctica

### Enunciado

Se te da el siguiente módulo de validación de contraseñas:

```c
// password_validator.h
#ifndef PASSWORD_VALIDATOR_H
#define PASSWORD_VALIDATOR_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PW_OK = 0,
    PW_ERR_NULL,
    PW_ERR_TOO_SHORT,
    PW_ERR_TOO_LONG,
    PW_ERR_NO_UPPER,
    PW_ERR_NO_LOWER,
    PW_ERR_NO_DIGIT,
    PW_ERR_NO_SPECIAL,
    PW_ERR_COMMON,
    PW_ERR_SEQUENCE,
    PW_ERR_REPEATED
} PasswordError;

typedef struct {
    size_t min_length;
    size_t max_length;
    bool   require_upper;
    bool   require_lower;
    bool   require_digit;
    bool   require_special;
    bool   check_common;
    bool   check_sequences;
    size_t max_repeated;     // Max repeated consecutive chars (0=unlimited)
} PasswordPolicy;

// Default policy: min 8, max 128, all checks enabled, max_repeated=3
PasswordPolicy pw_default_policy(void);

// Validate password against policy. Returns first error found.
PasswordError pw_validate(const char *password, const PasswordPolicy *policy);

// Get human-readable error message.
const char *pw_error_string(PasswordError err);

// Calculate password strength (0-100).
int pw_strength(const char *password);

#endif
```

```c
// password_validator.c
#include "password_validator.h"
#include <string.h>
#include <ctype.h>

// Common passwords (simplified list)
static const char *COMMON_PASSWORDS[] = {
    "password", "123456", "12345678", "qwerty", "abc123",
    "monkey", "master", "dragon", "111111", "baseball",
    "iloveyou", "trustno1", "sunshine", "princess", "welcome",
    NULL
};

PasswordPolicy pw_default_policy(void) {
    return (PasswordPolicy){
        .min_length      = 8,
        .max_length      = 128,
        .require_upper   = true,
        .require_lower   = true,
        .require_digit   = true,
        .require_special = true,
        .check_common    = true,
        .check_sequences = true,
        .max_repeated    = 3
    };
}

static bool is_common(const char *password) {
    // Case-insensitive comparison
    for (int i = 0; COMMON_PASSWORDS[i]; i++) {
        if (strcasecmp(password, COMMON_PASSWORDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_sequence(const char *password) {
    size_t len = strlen(password);
    if (len < 3) return false;

    for (size_t i = 0; i < len - 2; i++) {
        // Ascending sequence: abc, 123
        if (password[i] + 1 == password[i+1] &&
            password[i+1] + 1 == password[i+2]) {
            return true;
        }
        // Descending sequence: cba, 321
        if (password[i] - 1 == password[i+1] &&
            password[i+1] - 1 == password[i+2]) {
            return true;
        }
    }
    return false;
}

static bool has_repeated(const char *password, size_t max) {
    if (max == 0) return false;  // Unlimited

    size_t len = strlen(password);
    size_t count = 1;

    for (size_t i = 1; i < len; i++) {
        if (password[i] == password[i-1]) {
            count++;
            if (count > max) return true;
        } else {
            count = 1;
        }
    }
    return false;
}

PasswordError pw_validate(const char *password, const PasswordPolicy *policy) {
    if (!password) return PW_ERR_NULL;

    PasswordPolicy pol;
    if (policy) {
        pol = *policy;
    } else {
        pol = pw_default_policy();
    }

    size_t len = strlen(password);

    if (len < pol.min_length) return PW_ERR_TOO_SHORT;
    if (len > pol.max_length) return PW_ERR_TOO_LONG;

    if (pol.require_upper) {
        bool found = false;
        for (size_t i = 0; i < len; i++) {
            if (isupper((unsigned char)password[i])) { found = true; break; }
        }
        if (!found) return PW_ERR_NO_UPPER;
    }

    if (pol.require_lower) {
        bool found = false;
        for (size_t i = 0; i < len; i++) {
            if (islower((unsigned char)password[i])) { found = true; break; }
        }
        if (!found) return PW_ERR_NO_LOWER;
    }

    if (pol.require_digit) {
        bool found = false;
        for (size_t i = 0; i < len; i++) {
            if (isdigit((unsigned char)password[i])) { found = true; break; }
        }
        if (!found) return PW_ERR_NO_DIGIT;
    }

    if (pol.require_special) {
        bool found = false;
        for (size_t i = 0; i < len; i++) {
            if (!isalnum((unsigned char)password[i])) { found = true; break; }
        }
        if (!found) return PW_ERR_NO_SPECIAL;
    }

    if (pol.check_common && is_common(password)) {
        return PW_ERR_COMMON;
    }

    if (pol.check_sequences && has_sequence(password)) {
        return PW_ERR_SEQUENCE;
    }

    if (pol.max_repeated > 0 && has_repeated(password, pol.max_repeated)) {
        return PW_ERR_REPEATED;
    }

    return PW_OK;
}

const char *pw_error_string(PasswordError err) {
    switch (err) {
        case PW_OK:             return "Password is valid";
        case PW_ERR_NULL:       return "Password is NULL";
        case PW_ERR_TOO_SHORT:  return "Password is too short";
        case PW_ERR_TOO_LONG:   return "Password is too long";
        case PW_ERR_NO_UPPER:   return "Password needs uppercase letter";
        case PW_ERR_NO_LOWER:   return "Password needs lowercase letter";
        case PW_ERR_NO_DIGIT:   return "Password needs a digit";
        case PW_ERR_NO_SPECIAL: return "Password needs a special character";
        case PW_ERR_COMMON:     return "Password is too common";
        case PW_ERR_SEQUENCE:   return "Password contains character sequence";
        case PW_ERR_REPEATED:   return "Password has too many repeated characters";
        default:                return "Unknown error";
    }
}

int pw_strength(const char *password) {
    if (!password) return 0;

    size_t len = strlen(password);
    int score = 0;

    // Length score (up to 30 points)
    if (len >= 8)  score += 10;
    if (len >= 12) score += 10;
    if (len >= 16) score += 10;

    // Character diversity (up to 40 points)
    bool has_upper = false, has_lower = false;
    bool has_digit = false, has_special = false;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = password[i];
        if (isupper(c))       has_upper = true;
        else if (islower(c))  has_lower = true;
        else if (isdigit(c))  has_digit = true;
        else                  has_special = true;
    }

    if (has_upper)   score += 10;
    if (has_lower)   score += 10;
    if (has_digit)   score += 10;
    if (has_special) score += 10;

    // Penalty for common (-20)
    if (is_common(password)) score -= 20;

    // Penalty for sequences (-10)
    if (len >= 3 && has_sequence(password)) score -= 10;

    // Unique characters bonus (up to 20 points)
    bool seen[256] = {false};
    int unique = 0;
    for (size_t i = 0; i < len; i++) {
        if (!seen[(unsigned char)password[i]]) {
            seen[(unsigned char)password[i]] = true;
            unique++;
        }
    }

    if (unique >= 6)  score += 10;
    if (unique >= 10) score += 10;

    // Clamp to 0-100
    if (score < 0)   score = 0;
    if (score > 100)  score = 100;

    return score;
}
```

### Tarea

1. **Escribe un archivo `test_password.c`** con tests usando assert(). NO intentes cubrir todo — escribe tests "normales" como lo harías naturalmente (happy path + 2-3 errores).

2. **Compila con `--coverage`** y genera reporte con `gcov -f -b`.

3. **Analiza el reporte**: para cada función, documenta:
   - Line coverage %
   - Branch coverage %
   - Qué ramas específicas no están cubiertas
   - Riesgo de cada rama no cubierta (alto/medio/bajo)

4. **Crea un plan de mejora** con tests adicionales, priorizados por riesgo.

5. **Implementa los tests de mejora** y re-ejecuta gcov. Documenta el antes/después.

6. **Genera reporte lcov+genhtml** con y sin los tests de mejora. Compara visualmente.

7. Responde: ¿Hay alguna rama que decidiste **no testear**? ¿Por qué?

---

## 21. Ejercicios

### Ejercicio 1: Interpretar un .gcov (20 min)

Dado este `.gcov` de una función `validate_email`:

```
       50:   10:bool validate_email(const char *email) {
       50:   11:    if (!email) return false;
branch  0 taken 2%
branch  1 taken 98%
       49:   12:    size_t len = strlen(email);
       49:   13:    if (len < 3 || len > 254) return false;
branch  0 taken 4%
branch  1 taken 96%
branch  2 taken 0%
branch  3 taken 100%
       47:   14:    const char *at = strchr(email, '@');
       47:   15:    if (!at) return false;
branch  0 taken 6%
branch  1 taken 94%
       44:   16:    if (at == email) return false;
branch  0 taken 2%
branch  1 taken 98%
       43:   17:    if (at[1] == '\0') return false;
branch  0 taken 2%
branch  1 taken 98%
       42:   18:    const char *dot = strrchr(at, '.');
       42:   19:    if (!dot) return false;
branch  0 taken 5%
branch  1 taken 95%
       40:   20:    if (dot == at + 1) return false;
branch  0 taken 0%                          ←
branch  1 taken 100%
       40:   21:    if (strlen(dot + 1) < 2) return false;
branch  0 taken 0%                          ←
branch  1 taken 100%
       40:   22:    return true;
```

**Tareas**:
- a) ¿Cuál es el line coverage? ¿Y el branch coverage?
- b) ¿Cuáles son las 2 ramas no cubiertas? Escribe inputs que las cubran
- c) ¿Qué validación falta completamente? (piensa en emails reales que deberían fallar)
- d) Clasifica cada validación por riesgo (en un sistema real de registro)

### Ejercicio 2: 100% es mentira (25 min)

Implementa esta función y escribe tests que den 100% de line coverage y 100% de branch coverage pero que **no detecten** un bug que tú introduces intencionalmente:

```c
// Implementa: calcular el promedio de un array de doubles
// Manejar: array vacío (retorna 0), NULL (retorna 0)
double average(const double *arr, size_t len);
```

**Tareas**:
- a) Implementa `average()` **con un bug sutil** (por ejemplo, off-by-one, no manejar NaN, overflow con números grandes)
- b) Escribe tests con 100% line y branch coverage que **no detecten** el bug
- c) Demuestra el bug con un test específico que lo expone
- d) Reflexiona: ¿qué tipo de cobertura habría revelado el bug?

### Ejercicio 3: Auditoría de cobertura real (30 min)

Toma **cualquier archivo .c** que hayas escrito en bloques anteriores de este curso (o el ring_buffer del T01).

**Tareas**:
- a) Compila con `--coverage` y ejecuta los tests existentes
- b) Genera reporte con `gcov -f -b`
- c) Escribe una auditoría formal siguiendo el formato de la sección 19:
  - Resumen de métricas
  - Análisis por función (líneas, ramas, qué falta, riesgo)
  - Plan de mejora priorizado
- d) Implementa las 3 mejoras de mayor prioridad
- e) Re-ejecuta y documenta el antes/después

### Ejercicio 4: Branch vs Condition vs MC/DC manual (25 min)

Dado:

```c
bool should_alert(int temp, int pressure, bool manual_override) {
    if ((temp > 100 || pressure > 500) && !manual_override) {
        return true;
    }
    return false;
}
```

**Tareas**:
- a) Enumera todas las ramas (branch coverage): ¿cuántas hay? ¿Cuántos tests mínimos para 100%?
- b) Enumera todas las condiciones atómicas: ¿cuántas hay? Escribe tests para 100% condition coverage
- c) Construye la tabla MC/DC: ¿cuántos tests mínimos? ¿Cuáles?
- d) Compila con `gcov -b` y verifica que tus tests de MC/DC dan 100% branch coverage
- e) Descompone la expresión en `if` anidados y verifica que branch coverage ahora es equivalente a MC/DC

---

**Navegación**:
- ← Anterior: [T01 — gcov y lcov](../T01_gcov_y_lcov/README.md)
- → Siguiente: [T03 — Cobertura en CI](../T03_Cobertura_en_CI/README.md)
- ↑ Sección: [S04 — Cobertura en C](../README.md)
