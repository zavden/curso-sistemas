# T01 - Concepto de Property-Based Testing

## Índice

1. [El problema: los tests basados en ejemplos mienten por omisión](#1-el-problema-los-tests-basados-en-ejemplos-mienten-por-omisión)
2. [Qué es Property-Based Testing](#2-qué-es-property-based-testing)
3. [Propiedades vs ejemplos: la diferencia filosófica](#3-propiedades-vs-ejemplos-la-diferencia-filosófica)
4. [Anatomía de un property test](#4-anatomía-de-un-property-test)
5. [Generación aleatoria de inputs](#5-generación-aleatoria-de-inputs)
6. [Propiedades invariantes: qué verificar](#6-propiedades-invariantes-qué-verificar)
7. [Shrinking automático: encontrar el caso mínimo](#7-shrinking-automático-encontrar-el-caso-mínimo)
8. [El ciclo de un property test](#8-el-ciclo-de-un-property-test)
9. [Tipos de propiedades](#9-tipos-de-propiedades)
10. [Propiedades algebraicas](#10-propiedades-algebraicas)
11. [Round-trip properties](#11-round-trip-properties)
12. [Invariantes de estructura de datos](#12-invariantes-de-estructura-de-datos)
13. [Oracle testing: comparar con implementación de referencia](#13-oracle-testing-comparar-con-implementación-de-referencia)
14. [Idempotencia y otras propiedades operacionales](#14-idempotencia-y-otras-propiedades-operacionales)
15. [Cuándo usar y cuándo NO usar property testing](#15-cuándo-usar-y-cuándo-no-usar-property-testing)
16. [Property testing en el ecosistema Rust](#16-property-testing-en-el-ecosistema-rust)
17. [Comparación con C y Go](#17-comparación-con-c-y-go)
18. [Historia y origen: de QuickCheck a hoy](#18-historia-y-origen-de-quickcheck-a-hoy)
19. [Errores conceptuales comunes](#19-errores-conceptuales-comunes)
20. [Ejemplo completo: descubriendo bugs con properties](#20-ejemplo-completo-descubriendo-bugs-con-properties)
21. [Programa de práctica](#21-programa-de-práctica)
22. [Ejercicios](#22-ejercicios)

---

## 1. El problema: los tests basados en ejemplos mienten por omisión

Todo lo que hemos visto hasta ahora — unit tests, integration tests, doc tests — comparte una limitación fundamental: el programador **elige** los inputs. Esto crea un sesgo inevitable:

```rust
#[test]
fn test_sort() {
    let mut v = vec![3, 1, 4, 1, 5];
    sort(&mut v);
    assert_eq!(v, vec![1, 1, 3, 4, 5]);
}
```

Este test pasa. ¿Probamos que `sort` funciona? No. Probamos que funciona para **un** input específico de 5 elementos que el programador eligió. ¿Qué hay de:

```
Inputs que el programador no pensó probar:

  vec![]                          ← Vacío
  vec![1]                         ← Un solo elemento
  vec![1, 1, 1, 1, 1]            ← Todos iguales
  vec![1, 2, 3, 4, 5]            ← Ya ordenado
  vec![5, 4, 3, 2, 1]            ← Orden inverso
  vec![i32::MAX, i32::MIN, 0]    ← Extremos
  vec![0; 10_000]                ← Grande y repetido
  vec![generado aleatoriamente]   ← Patrones inesperados
```

Un buen programador podría añadir tests para algunos de estos. Pero nunca para todos. Y los bugs más interesantes viven en los inputs que **no imaginaste**:

```
El iceberg de testing:

            ╱╲
           ╱  ╲
          ╱ Lo ╲
         ╱ que  ╲
        ╱ probas-╲        ← 5-20 ejemplos elegidos a mano
       ╱  te      ╲
      ╱─────────────╲
     ╱                ╲
    ╱   Lo que NO      ╲
   ╱    probaste        ╲
  ╱                      ╲
 ╱    (donde viven los    ╲   ← Infinitos inputs posibles
╱      bugs reales)        ╲
╱────────────────────────────╲
```

### El sesgo del programador

```
Problema cognitivo de los example-based tests:

  1. El programador escribe la IMPLEMENTACIÓN
  2. El mismo programador escribe los TESTS
  3. Los tests reflejan el modelo mental del programador
  4. Los bugs están donde el modelo mental es INCORRECTO
  5. Por definición, el programador NO probará esos casos
     (si supiera que su modelo mental está mal, lo arreglaría)

  Resultado: los tests prueban los casos que YA sabes que funcionan.
  Los bugs viven en los casos que NO sabías que debías probar.
```

Property-Based Testing rompe este ciclo dejando que la **máquina** genere los inputs.

---

## 2. Qué es Property-Based Testing

Property-Based Testing (PBT) es un paradigma de testing donde:

1. Tú defines una **propiedad** que debe ser verdadera para **todos** los inputs válidos
2. El framework genera **cientos o miles** de inputs aleatorios
3. Para cada input, verifica que la propiedad se cumple
4. Si encuentra un input que viola la propiedad, lo **reduce** al caso mínimo que falla

```
Example-Based Testing:                Property-Based Testing:

  Tú eliges:                          Tú defines:
    input = [3, 1, 4]                   "Para todo vector v:
    expected = [1, 3, 4]                 sort(v) debe estar ordenado"

  El test verifica:                    El framework:
    sort([3,1,4]) == [1,3,4]            Genera: [3,1,4], [7,2], [], [1,1,1], ...
                                         (100+ inputs aleatorios)
  Cobertura: 1 caso                     Verifica propiedad en cada uno
                                         Reporta el primer fallo
                                         Reduce al caso mínimo
                                        Cobertura: 100+ casos
```

### Definición formal

```
Un property test tiene tres componentes:

  1. GENERADOR    → Produce inputs aleatorios del tipo correcto
                    "Dame vectores de enteros de longitud 0-100"

  2. PROPIEDAD    → Predicado que debe ser true para TODO input
                    "Después de sort, cada elemento es ≤ al siguiente"

  3. SHRINKING    → Cuando falla, reduce el input al caso mínimo
                    [47, -3, 91, 0, 12] falla
                    → [47, -3] falla
                    → [-3, 47]... no falla
                    → [-1, 0] falla
                    → Caso mínimo reportado: [-1, 0]
```

### El contrato implícito

En un test basado en ejemplos, el contrato es: "para este input específico, espero este output específico". En un property test, el contrato es más abstracto y poderoso: "para **cualquier** input válido, esta propiedad se cumple".

```rust
// Example-based: un contrato estrecho
#[test]
fn test_reverse() {
    assert_eq!(reverse("hello"), "olleh");
    assert_eq!(reverse(""), "");
    assert_eq!(reverse("a"), "a");
    // ¿Y los miles de otros strings posibles?
}

// Property-based: un contrato universal
// "Para cualquier string s, reverse(reverse(s)) == s"
// El framework prueba con: "", "a", "hello", "日本語",
// "aaaaaa", "\n\t", un string de 10000 chars, etc.
```

---

## 3. Propiedades vs ejemplos: la diferencia filosófica

### Un ejemplo verifica UN punto

```
                output
                  ↑
                  │     ✓ Aquí verificamos
                  │     ●
                  │
                  │
                  └──────────────────→ input

  Un test de ejemplo dice:
    "Para input=5, output=25"
    Solo sabemos que UN punto del espacio es correcto.
```

### Una propiedad verifica una REGIÓN

```
                output
                  ↑
                  │ ✓✓✓✓✓✓✓✓✓✓✓✓
                  │ ✓✓✓✓✓✓✓✓✓✓✓✓
                  │ ✓✓✓✓✓✓✓✓✓✓✓✓
                  │ ✓✓✓✓✓✓✓✓✓✓✓✓
                  └──────────────────→ input

  Un property test dice:
    "Para CUALQUIER input, output tiene esta forma"
    Verificamos que una región entera es correcta.
    (no todos los puntos, pero muchos muestreados)
```

### La pirámide de especificidad

```
Más específico ↑
               │  Example test:  sort([3,1,4]) == [1,3,4]
               │                 (UN caso concreto)
               │
               │  Property test: para todo v, sort(v) está ordenado
               │                 Y sort(v) tiene los mismos elementos que v
               │                 (TODOS los casos posibles)
               │
               │  Proof:         sort(v) es correcto por demostración formal
Más general    │                 (CERTEZA matemática)
               ↓
```

Los property tests están entre los tests de ejemplo (específicos, fáciles) y las pruebas formales (completas, difíciles). Son un punto dulce pragmático.

---

## 4. Anatomía de un property test

Un property test en pseudocódigo tiene esta estructura:

```
para i en 1..NUM_CASES:
    input = generar_aleatorio()
    resultado = función_bajo_test(input)
    assert propiedad(input, resultado)

si algún assert falla:
    input_reducido = shrink(input)
    reportar(input_reducido)
```

### En Rust con proptest (vista previa)

```rust
use proptest::prelude::*;

proptest! {
    #[test]
    fn test_sort_preserves_length(ref v in any::<Vec<i32>>()) {
        let mut sorted = v.clone();
        sorted.sort();
        // PROPIEDAD: sort no cambia la longitud
        assert_eq!(sorted.len(), v.len());
    }
}
```

### Desglose pieza por pieza

```
proptest! {
    #[test]
    fn test_sort_preserves_length(
        ref v in any::<Vec<i32>>()     ← GENERADOR
    ) {                                   Produce Vec<i32> aleatorios
        let mut sorted = v.clone();
        sorted.sort();                  ← CÓDIGO bajo test

        assert_eq!(                     ← PROPIEDAD
            sorted.len(),                 "La longitud se preserva"
            v.len()
        );
    }
}

Si falla con v = [47, -3, 91, 0, 12]:

  SHRINKING automático:
    [47, -3, 91, 0, 12] → falla
    [47, -3, 91] → ¿falla? → No → no es este subconjunto
    [47, -3] → ¿falla? → Sí → seguir reduciendo
    [0, -3] → ¿falla? → Sí → seguir
    [0, -1] → ¿falla? → Sí → caso mínimo

  Reporte: "Failing input: [0, -1]"
```

### Los tres ingredientes esenciales

```
┌─────────────────────────────────────────────────────┐
│                Property Test                         │
│                                                     │
│  ┌──────────────┐                                   │
│  │  GENERADOR    │  "Dame datos aleatorios válidos"  │
│  │              │                                   │
│  │  any::<i32>() │  Produce: 0, -1, 42, MAX, MIN... │
│  │  any::<String>│  Produce: "", "a", "hello", ...  │
│  │  vec![0..100] │  Produce: vec de 0 a 100 elems   │
│  └──────┬───────┘                                   │
│         │                                           │
│         ▼                                           │
│  ┌──────────────┐                                   │
│  │  PROPIEDAD    │  "Esto debe ser verdad siempre"   │
│  │              │                                   │
│  │  f(x) == f(x)│  Determinismo                     │
│  │  len(sort(v))│  Preservación                     │
│  │  == len(v)   │                                   │
│  │  decode(     │  Round-trip                        │
│  │   encode(x)) │                                   │
│  │  == x        │                                   │
│  └──────┬───────┘                                   │
│         │ si falla                                  │
│         ▼                                           │
│  ┌──────────────┐                                   │
│  │  SHRINKING    │  "Encontrar el caso MÍNIMO"       │
│  │              │                                   │
│  │  [47,-3,91]  │                                   │
│  │  → [47,-3]   │                                   │
│  │  → [0,-1]    │  ← Caso mínimo que falla          │
│  └──────────────┘                                   │
└─────────────────────────────────────────────────────┘
```

---

## 5. Generación aleatoria de inputs

El generador es el corazón del property test. Necesita producir datos que sean:

- **Válidos**: del tipo correcto para la función bajo test
- **Variados**: cubrir edge cases, no solo el rango "feliz"
- **Reducibles**: permitir que el shrinker encuentre el caso mínimo

### Qué genera un buen generador

```
Para i32:
  ┌─────────────────────────────────────────────────┐
  │ Edge cases:  0, 1, -1, i32::MAX, i32::MIN      │
  │ Pequeños:    2, 3, -2, -3, 10, -10              │
  │ Medianos:    42, -99, 1000, -5000               │
  │ Grandes:     1_000_000, -1_000_000              │
  │ Extremos:    i32::MAX-1, i32::MIN+1             │
  │ Aleatorios:  cualquier valor en el rango         │
  └─────────────────────────────────────────────────┘

Para String:
  ┌─────────────────────────────────────────────────┐
  │ Edge cases:  "", " ", "\n", "\0"                │
  │ ASCII:       "a", "hello", "test123"            │
  │ Unicode:     "日本語", "émojis🎉", "Ñ"          │
  │ Especiales:  "\t\r\n", "null\0byte"             │
  │ Largos:      "a".repeat(10000)                  │
  │ Mixtos:      "Hello世界\n\ttab"                  │
  └─────────────────────────────────────────────────┘

Para Vec<T>:
  ┌─────────────────────────────────────────────────┐
  │ Longitudes:  0, 1, 2, 10, 100, 1000             │
  │ Contenido:   todos iguales, todos diferentes     │
  │ Patrones:    ordenado, inverso, alternante       │
  │ Extremos:    vec de un solo elemento repetido    │
  └─────────────────────────────────────────────────┘
```

### Generación con sesgo hacia edge cases

Un buen framework de PBT no genera datos uniformemente al azar. Sesga la generación hacia valores que históricamente causan bugs:

```
Distribución de generación para i32:

  Uniform random:
    ───────────────────────────────────────
    Cada valor igual de probable.
    Casi nunca genera 0, MAX, MIN.
    Ineficiente para encontrar bugs.

  Con sesgo (lo que hacen proptest/quickcheck):
    ▁▁▁▁▂▃▅▇█▇▅▃▂▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁▁
    ↑        ↑
    MIN      0    Valores cerca de 0 son más frecuentes
                  Edge cases (0, ±1, MAX, MIN) aparecen a menudo
                  Pero también genera valores aleatorios grandes
```

### Restricción de generadores

A veces necesitas inputs que cumplan precondiciones:

```
"Generar enteros positivos":
  No: any::<i32>()              ← Genera negativos también
  Sí: 1..i32::MAX               ← Solo positivos

"Generar vectores no vacíos":
  No: any::<Vec<i32>>()         ← Genera vec![]
  Sí: vec![any::<i32>(); 1..100] ← Al menos 1 elemento

"Generar emails válidos":
  No: any::<String>()           ← Genera "asdfgh", no es email
  Sí: "[a-z]{3,10}@[a-z]{3,5}\\.[a-z]{2,3}"  ← Genera "abc@def.com"
```

---

## 6. Propiedades invariantes: qué verificar

La parte más difícil de PBT no es el framework — es **pensar en las propiedades**. ¿Qué debe ser verdad para **todos** los inputs posibles?

### Categorías de propiedades

```
┌─────────────────────────────────────────────────────┐
│           Tipos de propiedades                       │
│                                                     │
│  1. ROUND-TRIP                                       │
│     decode(encode(x)) == x                           │
│     deserialize(serialize(x)) == x                   │
│     decompress(compress(x)) == x                     │
│                                                     │
│  2. INVARIANTES                                      │
│     sort(v) está ordenado                            │
│     len(sort(v)) == len(v)                           │
│     tree.insert(x) → tree.contains(x)               │
│                                                     │
│  3. ALGEBRAICAS                                      │
│     a + b == b + a  (conmutatividad)                 │
│     (a + b) + c == a + (b + c)  (asociatividad)      │
│     a + 0 == a  (identidad)                          │
│                                                     │
│  4. ORACLE (comparación)                             │
│     mi_sort(v) == std_sort(v)                        │
│     mi_parser(s) == reference_parser(s)              │
│                                                     │
│  5. IDEMPOTENCIA                                     │
│     f(f(x)) == f(x)                                  │
│     normalize(normalize(s)) == normalize(s)          │
│                                                     │
│  6. PRESERVACIÓN                                     │
│     sort no cambia los elementos                     │
│     map preserva la longitud                         │
│     filter solo quita, no modifica                   │
│                                                     │
│  7. RELACIÓN ENTRE OPERACIONES                       │
│     push(x) entonces pop() == x                      │
│     insert(k,v) entonces get(k) == v                 │
│     después de remove(k), contains(k) == false       │
└─────────────────────────────────────────────────────┘
```

### Ejemplo: propiedades de sort

```rust
// Para la función: fn sort(v: &mut Vec<i32>)

// Propiedad 1: El resultado está ordenado
// "Para todo v: sort(v) está ordenado"
fn prop_sorted(v: Vec<i32>) -> bool {
    let mut sorted = v.clone();
    sorted.sort();
    sorted.windows(2).all(|w| w[0] <= w[1])
}

// Propiedad 2: La longitud se preserva
// "Para todo v: len(sort(v)) == len(v)"
fn prop_length_preserved(v: Vec<i32>) -> bool {
    let mut sorted = v.clone();
    let original_len = v.len();
    sorted.sort();
    sorted.len() == original_len
}

// Propiedad 3: Los elementos se preservan (permutación)
// "Para todo v: sort(v) es una permutación de v"
fn prop_is_permutation(v: Vec<i32>) -> bool {
    let mut sorted = v.clone();
    sorted.sort();
    let mut original = v;
    original.sort();  // Ambos ordenados deben ser iguales
    sorted == original
}

// Propiedad 4: Idempotencia
// "Para todo v: sort(sort(v)) == sort(v)"
fn prop_idempotent(v: Vec<i32>) -> bool {
    let mut v1 = v.clone();
    v1.sort();
    let mut v2 = v1.clone();
    v2.sort();
    v1 == v2
}

// Propiedad 5: El mínimo está primero
// "Para todo v no vacío: sort(v)[0] == min(v)"
fn prop_minimum_first(v: Vec<i32>) -> bool {
    if v.is_empty() { return true; }
    let mut sorted = v.clone();
    sorted.sort();
    sorted[0] == *v.iter().min().unwrap()
}
```

Estas 5 propiedades juntas especifican `sort` casi completamente. Si todas pasan para miles de inputs aleatorios, es extremadamente probable que la implementación sea correcta.

---

## 7. Shrinking automático: encontrar el caso mínimo

El shrinking es lo que hace que PBT sea **práctico**. Sin shrinking, un fallo reportaría:

```
FAILED: input = [47291, -83, 0, 19482, -7, 33, 0, -29471, 8, 102, ...]
        (un vector de 847 elementos)
```

¿Cómo debuggeas eso? Con shrinking, el reporte es:

```
FAILED: input = [1, 0]
        (el caso MÍNIMO que reproduce el bug)
```

### Cómo funciona el shrinking

```
Shrinking de un Vec<i32>:

  Input que falló: [47, -3, 91, 0, 12]

  Paso 1: Intentar sublistas
    [47, -3, 91, 0]  → ¿falla? Sí → seguir con esta
    [47, -3, 91]     → ¿falla? Sí → seguir
    [47, -3]         → ¿falla? Sí → seguir
    [47]             → ¿falla? No → volver a [47, -3]

  Paso 2: Reducir valores individuales
    [47, -3] → [0, -3]  → ¿falla? Sí → seguir
    [0, -3]  → [0, -1]  → ¿falla? Sí → seguir
    [0, -1]  → [0, 0]   → ¿falla? No → volver a [0, -1]

  Resultado final: [0, -1]
  Este es el caso MÍNIMO que reproduce el bug.
```

### Estrategias de shrinking por tipo

```
Shrinking de i32:
  47 → 0 → ¿falla? → 23 → ¿falla? → 11 → ... → valor mínimo

  Intenta: 0, luego valores más cercanos a 0
  Binario: divide por 2 repetidamente

Shrinking de String:
  "Hello World" → "Hello" → "Hel" → "H" → ""
  También intenta: reemplazar caracteres por 'a', eliminar chars

Shrinking de Vec<T>:
  [a, b, c, d, e]
  → Remover elementos: [a, c, e], [b, d], [a, b, c], ...
  → Reducir cada elemento: [shrink(a), b, c, d, e]
  → Combinaciones

Shrinking de tuplas:
  (a, b, c)
  → (shrink(a), b, c)
  → (a, shrink(b), c)
  → (a, b, shrink(c))
  → Combinaciones
```

### Por qué es tan valioso

```
Sin shrinking:
  "Tu test falló con input: vec de 500 enteros aleatorios"
  → Debuggear: ¿cuáles de los 500 causan el bug? ¿Es la combinación?
  → Tiempo: horas

Con shrinking:
  "Tu test falló con input: [0, -1]"
  → Debuggear: solo 2 elementos, el patrón es obvio
  → Tiempo: segundos

El shrinking convierte un FALLO ALEATORIO en un CASO DE TEST MÍNIMO.
Es como tener un asistente que te dice:
  "Encontré un bug, y este es el ejemplo más simple que lo demuestra"
```

### Shrinking determinístico vs no determinístico

```
QuickCheck (Haskell/Rust):
  Shrinking no determinístico
  Cada ejecución puede dar un caso mínimo diferente
  Depende del generador y la semilla aleatoria

proptest (Rust):
  Shrinking determinístico
  Dado el mismo fallo, siempre produce el mismo caso mínimo
  Guarda la semilla en un archivo para reproducibilidad
  → Más predecible y reproducible
```

---

## 8. El ciclo de un property test

### Flujo completo de ejecución

```
cargo test ejecuta un property test:

  ┌─────────────────────────────────────────────────────┐
  │  Configuración                                      │
  │  ├── Número de casos: 256 (default en proptest)     │
  │  ├── Semilla: aleatoria o de archivo                │
  │  └── Max shrink iters: 1024                         │
  └──────────────────────┬──────────────────────────────┘
                         │
                         ▼
  ┌─────────────────────────────────────────────────────┐
  │  Iteración 1..256                                   │
  │                                                     │
  │  Para cada iteración:                               │
  │    1. Generar input aleatorio                       │
  │    2. Ejecutar la función bajo test                 │
  │    3. Verificar la propiedad                        │
  │       ├── PASS → siguiente iteración                │
  │       └── FAIL → ir a SHRINKING                     │
  └──────────────────────┬──────────────────────────────┘
                         │
            ┌────────────┴────────────┐
            │ Todas pasaron           │ Alguna falló
            ▼                         ▼
  ┌──────────────────┐   ┌──────────────────────────────┐
  │  TEST PASSED      │   │  SHRINKING                    │
  │                  │   │                              │
  │  "ok (256 cases  │   │  Reducir el input que falló: │
  │   tested)"       │   │    [47,-3,91] → falla         │
  └──────────────────┘   │    [47,-3] → falla             │
                         │    [0,-1] → falla              │
                         │    [0,0] → pasa                │
                         │    → Mínimo: [0,-1]            │
                         └──────────────┬───────────────┘
                                        │
                                        ▼
                         ┌──────────────────────────────┐
                         │  TEST FAILED                  │
                         │                              │
                         │  Minimal failing input:       │
                         │    [0, -1]                    │
                         │                              │
                         │  Seed guardada en:            │
                         │    proptest-regressions/      │
                         │    mi_test.txt                │
                         └──────────────────────────────┘
```

### Reproducibilidad con seeds

```
Cuando un property test falla, proptest guarda la semilla:

  proptest-regressions/
  └── mi_modulo/
      └── mi_test.txt

  Contenido del archivo:
    # Seed for test mi_modulo::mi_test
    cc 86a8b0a6d0da0e7e3c...

  La próxima vez que ejecutes cargo test,
  proptest PRIMERO re-ejecuta los casos guardados.
  → El fallo es reproducible aunque sea "aleatorio"
  → Commiteando proptest-regressions/, CI también lo reproduce
```

---

## 9. Tipos de propiedades

### Tabla de tipos con ejemplos

| Tipo | Patrón | Ejemplo |
|------|--------|---------|
| **Round-trip** | `decode(encode(x)) == x` | JSON serializar/deserializar |
| **Invariante** | `sort(v)` está ordenado | Postcondición de sort |
| **Preservación** | `len(sort(v)) == len(v)` | Sort no pierde elementos |
| **Algebraica** | `a + b == b + a` | Conmutatividad |
| **Oracle** | `mi_fn(x) == ref_fn(x)` | Comparar con referencia |
| **Idempotente** | `f(f(x)) == f(x)` | Normalizar dos veces |
| **Inversa** | `f(g(x)) == x` | Encriptar/desencriptar |
| **Inductiva** | `f(x+1) en función de f(x)` | Fibonacci |
| **No-crash** | `f(x)` no paniquea | Robustez |
| **Tamaño** | `len(f(x)) <= K * len(x)` | Compresión no explota |

### Elegir la propiedad correcta

```
¿Qué propiedad debo usar?

  ¿La función tiene inversa? (encode/decode, compress/decompress)
  └─ SÍ → ROUND-TRIP
       decode(encode(x)) == x

  ¿Hay una implementación de referencia? (std::sort, otro parser)
  └─ SÍ → ORACLE
       mi_sort(v) == std::sort(v)

  ¿La función transforma datos? (sort, filter, map)
  └─ SÍ → INVARIANTE + PRESERVACIÓN
       sort(v) está ordenado
       sort(v) tiene los mismos elementos

  ¿La función es matemática? (add, multiply, matrix ops)
  └─ SÍ → ALGEBRAICA
       a + b == b + a
       (a + b) + c == a + (b + c)

  ¿La función normaliza? (trim, lowercase, deduplicate)
  └─ SÍ → IDEMPOTENCIA
       f(f(x)) == f(x)

  ¿No encuentro ninguna propiedad específica?
  └─ NO-CRASH
       "Para todo input válido, la función no paniquea"
       (Es la propiedad mínima pero ya es útil)
```

---

## 10. Propiedades algebraicas

Las propiedades algebraicas verifican que una operación cumple leyes matemáticas:

### Conmutatividad: a OP b == b OP a

```rust
// Propiedad: la suma es conmutativa
// Para todo a, b: add(a, b) == add(b, a)
fn prop_add_commutative(a: i32, b: i32) -> bool {
    wrapping_add(a, b) == wrapping_add(b, a)
}

// Propiedad: merge de dos conjuntos es conmutativo
// Para todo s1, s2: merge(s1, s2) == merge(s2, s1)
fn prop_merge_commutative(s1: HashSet<i32>, s2: HashSet<i32>) -> bool {
    merge(&s1, &s2) == merge(&s2, &s1)
}
```

### Asociatividad: (a OP b) OP c == a OP (b OP c)

```rust
// Propiedad: la concatenación de strings es asociativa
fn prop_concat_associative(a: String, b: String, c: String) -> bool {
    let left = format!("{}{}{}", a, b, c);
    let right = format!("{}{}{}", a, b, c);
    left == right
    // O más explícito:
    // concat(concat(a, b), c) == concat(a, concat(b, c))
}
```

### Identidad: a OP identity == a

```rust
// Propiedad: sumar 0 no cambia el valor
fn prop_add_identity(a: i32) -> bool {
    a + 0 == a && 0 + a == a
}

// Propiedad: concatenar string vacío no cambia el string
fn prop_concat_identity(s: String) -> bool {
    format!("{}{}", s, "") == s && format!("{}{}", "", s) == s
}

// Propiedad: multiplicar por 1 no cambia la matriz
fn prop_matrix_identity(m: Matrix) -> bool {
    let identity = Matrix::identity(m.size());
    m.multiply(&identity) == m
}
```

### Distribuitvidad: a * (b + c) == a*b + a*c

```rust
// Propiedad: multiplicación distribuye sobre suma
// (cuidado con overflow y precisión flotante)
fn prop_distributive(a: i32, b: i32, c: i32) -> bool {
    let lhs = a.wrapping_mul(b.wrapping_add(c));
    let rhs = a.wrapping_mul(b).wrapping_add(a.wrapping_mul(c));
    lhs == rhs
}
```

### Involución: f(f(x)) == x

```rust
// Propiedad: negar dos veces retorna al original
fn prop_negate_involution(x: i32) -> bool {
    // Cuidado con i32::MIN donde -MIN overflow
    if x == i32::MIN { return true; }  // Skip edge case
    -(-x) == x
}

// Propiedad: reverse de reverse retorna al original
fn prop_reverse_involution(s: String) -> bool {
    let reversed: String = s.chars().rev().collect();
    let double_reversed: String = reversed.chars().rev().collect();
    double_reversed == s
}

// Propiedad: transponer dos veces retorna al original
fn prop_transpose_involution(m: Matrix) -> bool {
    m.transpose().transpose() == m
}
```

---

## 11. Round-trip properties

La propiedad más poderosa y más fácil de identificar. Si tienes una función y su inversa, el round-trip verifica ambas:

### Patrón general

```
          encode              decode
  datos ──────→ representación ──────→ datos
    x                                   x'

  Propiedad: x' == x
  Es decir:  decode(encode(x)) == x
```

### Ejemplos concretos

```rust
// Serialización JSON
fn prop_json_roundtrip(data: MyStruct) -> bool {
    let json = serde_json::to_string(&data).unwrap();
    let restored: MyStruct = serde_json::from_str(&json).unwrap();
    restored == data
}

// Compresión
fn prop_compress_roundtrip(data: Vec<u8>) -> bool {
    let compressed = compress(&data);
    let decompressed = decompress(&compressed);
    decompressed == data
}

// Base64
fn prop_base64_roundtrip(data: Vec<u8>) -> bool {
    let encoded = base64_encode(&data);
    let decoded = base64_decode(&encoded).unwrap();
    decoded == data
}

// URL encoding
fn prop_url_roundtrip(s: String) -> bool {
    let encoded = url_encode(&s);
    let decoded = url_decode(&encoded).unwrap();
    decoded == s
}

// Conversión de tipos
fn prop_to_string_roundtrip(n: i64) -> bool {
    let s = n.to_string();
    let parsed: i64 = s.parse().unwrap();
    parsed == n
}
```

### Round-trip parcial

A veces el round-trip no es perfecto porque se pierde información:

```rust
// Lossy: float → string → float puede perder precisión
fn prop_float_roundtrip(f: f64) -> bool {
    if f.is_nan() || f.is_infinite() { return true; }
    let s = format!("{}", f);
    let parsed: f64 = s.parse().unwrap();
    (parsed - f).abs() < 1e-10  // Aproximado, no exacto
}

// Lossy: HTML → texto pierde formato
fn prop_html_strip_roundtrip(text: String) -> bool {
    // NO es un round-trip (strip_html pierde información)
    // Pero podemos verificar una propiedad más débil:
    let html = wrap_in_html(&text);
    let stripped = strip_html(&html);
    stripped.contains(&text)  // Al menos el contenido está ahí
}
```

### El poder del round-trip

```
¿Por qué round-trip es tan poderoso?

  Un round-trip verifica AMBAS direcciones simultáneamente:

  Si encode tiene un bug:
    encode(x) produce datos incorrectos
    → decode(encode(x)) ≠ x → TEST FALLA

  Si decode tiene un bug:
    decode no puede reconstruir correctamente
    → decode(encode(x)) ≠ x → TEST FALLA

  Si ambos tienen bugs compensatorios:
    Extremadamente improbable que encode y decode tengan
    exactamente el mismo bug pero invertido
    → Casi seguro que TEST FALLA

  Con UN solo test verificas DOS funciones.
```

---

## 12. Invariantes de estructura de datos

Muchas estructuras de datos tienen invariantes internas que deben mantenerse después de cualquier operación:

### Invariantes de un BST (Binary Search Tree)

```
Un BST válido debe cumplir:

  1. Para cada nodo, todos los valores del subárbol izquierdo < valor del nodo
  2. Para cada nodo, todos los valores del subárbol derecho > valor del nodo
  3. Ambos subárboles son BSTs válidos (recursivo)

  Propiedad: después de CUALQUIER secuencia de insert/delete,
  el árbol sigue siendo un BST válido.
```

```rust
fn prop_bst_invariant_after_insert(initial: Vec<i32>, new_val: i32) -> bool {
    let mut tree = BinarySearchTree::new();
    for val in &initial {
        tree.insert(*val);
    }
    tree.insert(new_val);
    tree.is_valid_bst()  // Verifica el invariante
}

fn prop_bst_invariant_after_delete(values: Vec<i32>, to_delete: i32) -> bool {
    let mut tree = BinarySearchTree::new();
    for val in &values {
        tree.insert(*val);
    }
    tree.delete(to_delete);
    tree.is_valid_bst()
}
```

### Invariantes de un heap

```rust
// Un max-heap: cada padre es >= sus hijos
fn prop_heap_invariant(values: Vec<i32>) -> bool {
    let mut heap = MaxHeap::new();
    for val in &values {
        heap.push(*val);
    }

    // Después de construir, el invariante se mantiene
    heap.is_valid_heap()
}

fn prop_heap_max_is_correct(values: Vec<i32>) -> bool {
    if values.is_empty() { return true; }

    let mut heap = MaxHeap::new();
    for val in &values {
        heap.push(*val);
    }

    // El máximo del heap debe ser el máximo del vector
    heap.peek() == values.iter().max()
}
```

### Invariantes de un sorted vec

```rust
fn prop_sorted_vec_stays_sorted(ops: Vec<Operation>) -> bool {
    let mut sv = SortedVec::new();

    for op in &ops {
        match op {
            Operation::Insert(val) => sv.insert(*val),
            Operation::Remove(val) => { sv.remove(val); }
        }
        // DESPUÉS DE CADA operación, debe seguir ordenado
        if !sv.is_sorted() {
            return false;
        }
    }

    true
}
```

---

## 13. Oracle testing: comparar con implementación de referencia

Si ya existe una implementación conocida como correcta (en la stdlib, en otra crate, o una versión lenta pero obvia), puedes usarla como "oráculo":

### Patrón general

```
  input ────┬────→ mi_implementacion(input) ──→ result_a
            │
            └────→ referencia(input) ──────────→ result_b

  Propiedad: result_a == result_b
```

### Ejemplos

```rust
// Mi sort vs std sort
fn prop_my_sort_matches_std(mut v: Vec<i32>) -> bool {
    let mut expected = v.clone();
    expected.sort();           // Implementación de referencia (stdlib)

    my_sort(&mut v);           // Mi implementación

    v == expected
}

// Mi parser vs parser de referencia
fn prop_my_parser_matches_reference(input: String) -> bool {
    let my_result = my_json_parser::parse(&input);
    let ref_result = serde_json::from_str::<serde_json::Value>(&input);

    match (my_result, ref_result) {
        (Ok(a), Ok(b)) => a == b,
        (Err(_), Err(_)) => true,  // Ambos fallan: OK
        _ => false,                 // Uno falla y otro no: BUG
    }
}

// Implementación optimizada vs naíve
fn prop_fast_fibonacci_matches_naive(n: u32) -> bool {
    if n > 30 { return true; }  // La naíve es demasiado lenta
    fast_fibonacci(n) == naive_fibonacci(n)
}

fn naive_fibonacci(n: u32) -> u64 {
    match n {
        0 => 0,
        1 => 1,
        n => naive_fibonacci(n - 1) + naive_fibonacci(n - 2),
    }
}
```

### Oracle parcial (propiedad más débil)

A veces no tienes una referencia completa, pero puedes verificar propiedades parciales:

```rust
// No tengo otro parser de CSV, pero puedo verificar:
fn prop_csv_parser_consistency(rows: Vec<Vec<String>>) -> bool {
    let csv_string = rows_to_csv(&rows);
    let parsed = parse_csv(&csv_string);

    // Propiedad 1: misma cantidad de filas
    if parsed.len() != rows.len() { return false; }

    // Propiedad 2: misma cantidad de columnas por fila
    for (parsed_row, original_row) in parsed.iter().zip(rows.iter()) {
        if parsed_row.len() != original_row.len() { return false; }
    }

    true
}
```

---

## 14. Idempotencia y otras propiedades operacionales

### Idempotencia: f(f(x)) == f(x)

Aplicar la función una vez más no cambia el resultado:

```rust
// Normalizar texto
fn prop_normalize_idempotent(s: String) -> bool {
    let once = normalize(&s);
    let twice = normalize(&once);
    once == twice
}

// Formatear código
fn prop_format_idempotent(code: String) -> bool {
    let formatted = format_code(&code);
    let reformatted = format_code(&formatted);
    formatted == reformatted
}

// Deduplicar
fn prop_dedup_idempotent(v: Vec<i32>) -> bool {
    let once = dedup(&v);
    let twice = dedup(&once);
    once == twice
}

// Trim
fn prop_trim_idempotent(s: String) -> bool {
    let once = s.trim().to_string();
    let twice = once.trim().to_string();
    once == twice
}
```

### Monotonía: si a ≤ b entonces f(a) ≤ f(b)

```rust
// strlen es monótona respecto a la adición de caracteres
fn prop_strlen_monotone(s: String, c: char) -> bool {
    let extended = format!("{}{}", s, c);
    extended.len() >= s.len()
}

// Insertar en un conjunto no disminuye su tamaño
fn prop_insert_monotone(s: HashSet<i32>, val: i32) -> bool {
    let original_len = s.len();
    let mut s_copy = s;
    s_copy.insert(val);
    s_copy.len() >= original_len
}
```

### No-crash (propiedad mínima)

Cuando no puedes definir una propiedad más específica, "no crash" ya es valiosa:

```rust
// El parser no crashea con input arbitrario
fn prop_parser_no_crash(input: String) -> bool {
    // No nos importa el resultado, solo que no haga panic
    let _ = parse(&input);
    true  // Si llegamos aquí, no crasheó
}

// El formateador no crashea con datos arbitrarios
fn prop_formatter_no_crash(data: Vec<u8>) -> bool {
    let _ = format_data(&data);
    true
}
```

---

## 15. Cuándo usar y cuándo NO usar property testing

### Cuándo SÍ usar PBT

| Escenario | Por qué PBT ayuda |
|-----------|-------------------|
| Funciones puras (input → output) | Fácil definir propiedades |
| Serialización/deserialización | Round-trip natural |
| Parsers | Probar con inputs variados expone bugs |
| Algoritmos de ordenamiento/búsqueda | Invariantes claras |
| Estructuras de datos | Invariantes que mantener |
| Funciones matemáticas | Propiedades algebraicas |
| Conversiones de tipos | Round-trip |
| Codecs (encode/decode) | Round-trip |
| Cuando ya tienes una implementación de referencia | Oracle |

### Cuándo NO usar PBT

| Escenario | Por qué PBT no encaja |
|-----------|----------------------|
| Tests de UI | No hay propiedad clara |
| Tests de integración con BD | Generación de datos compleja |
| Tests de comportamiento con estado complejo | Difícil definir generadores |
| "El output debe ser exactamente este string" | Eso es un test de ejemplo |
| Funciones con efectos secundarios | Difícil verificar propiedades |
| APIs de red | Generación de inputs no ayuda |
| Funciones triviales | Overhead > beneficio |

### El sweet spot

```
                  Complejidad de la propiedad
                            ↑
  Demasiado complejo       │  ✗ No vale la pena:
  para definir propiedad   │    la propiedad es tan
                           │    compleja como el código
                           │
                           │  ✓ SWEET SPOT:
                           │    propiedad simple,
                           │    lógica compleja
                           │
  Propiedad trivial        │  ✗ No vale la pena:
  (f(x) no crashea)       │    test de ejemplo basta
                           └──────────────────────→
                             Complejidad de la implementación

  Máximo valor: cuando la PROPIEDAD es simple pero
  la IMPLEMENTACIÓN es compleja.

  Ejemplo:
    Propiedad: sort(v) está ordenado  ← Simple de escribir
    Implementación: quicksort         ← Compleja, propensa a bugs
```

### PBT complementa, no reemplaza

```
Pirámide de testing con PBT:

                    ╱╲
                   ╱  ╲
                  ╱ E2E╲
                 ╱      ╲
                ╱──────────╲
               ╱ Integration╲
              ╱    tests      ╲
             ╱──────────────────╲
            ╱                    ╲
           ╱  Property-Based ←──── ╲   Nueva capa
          ╱   Tests                 ╲  (no reemplaza nada,
         ╱                           ╲  se añade)
        ╱─────────────────────────────╲
       ╱                               ╲
      ╱     Example-Based Unit Tests     ╲
     ╱         (siguen siendo la base)    ╲
    ╱───────────────────────────────────────╲
```

---

## 16. Property testing en el ecosistema Rust

Rust tiene dos crates principales para PBT:

### proptest

```toml
[dev-dependencies]
proptest = "1"
```

```rust
use proptest::prelude::*;

proptest! {
    #[test]
    fn test_sort_preserves_length(ref v in any::<Vec<i32>>()) {
        let mut sorted = v.clone();
        sorted.sort();
        prop_assert_eq!(sorted.len(), v.len());
    }
}
```

Características:
- Shrinking **determinístico** (reproducible)
- Estrategias de generación componibles
- Guarda regressions automáticamente
- Soporte para regex en generación de strings
- API macro (`proptest!`) o funcional

### quickcheck

```toml
[dev-dependencies]
quickcheck = "1"
quickcheck_macros = "1"
```

```rust
use quickcheck_macros::quickcheck;

#[quickcheck]
fn test_sort_preserves_length(v: Vec<i32>) -> bool {
    let mut sorted = v.clone();
    sorted.sort();
    sorted.len() == v.len()
}
```

Características:
- Inspirado en QuickCheck de Haskell
- API más simple
- Shrinking basado en el trait `Arbitrary`
- Menos control sobre generadores

### Comparación rápida

| Aspecto | proptest | quickcheck |
|---------|----------|------------|
| **Shrinking** | Determinístico | No determinístico |
| **Generadores** | Estrategias componibles | Trait `Arbitrary` |
| **Strings** | Regex strategies | Random básico |
| **Regressions** | Archivo automático | Manual |
| **API** | Macro `proptest!` | Macro `#[quickcheck]` |
| **Popularidad** | Más popular | Más antiguo |
| **Config** | `ProptestConfig` | Parámetros en runtime |
| **Tipo retorno** | `prop_assert!` macros | `bool` o `TestResult` |

Ambos se estudian en detalle en T02 (proptest) y T04 (proptest vs quickcheck).

---

## 17. Comparación con C y Go

### C: No hay soporte nativo

En C no hay property testing integrado. Se necesitan bibliotecas externas:

```c
/* Con theft (librería de PBT para C) */
#include "theft.h"

/* Generador de arrays de enteros */
enum theft_alloc_res
int_array_alloc(struct theft *t, void *env, void **output) {
    size_t len = theft_random_choice(t, 100);
    int *arr = malloc(len * sizeof(int));
    for (size_t i = 0; i < len; i++) {
        arr[i] = (int)theft_random_choice(t, 2000) - 1000;
    }
    *output = arr;
    return THEFT_ALLOC_OK;
}

/* Propiedad: sort es idempotente */
enum theft_trial_res
prop_sort_idempotent(struct theft *t, void *arg) {
    int *arr = (int *)arg;
    size_t len = /* ... */;

    sort(arr, len);
    int *copy = memdup(arr, len * sizeof(int));
    sort(arr, len);

    if (memcmp(arr, copy, len * sizeof(int)) != 0) {
        free(copy);
        return THEFT_TRIAL_FAIL;
    }
    free(copy);
    return THEFT_TRIAL_PASS;
}

/* Problemas con C:
   - Manejo manual de memoria para generadores
   - No hay genéricos → un generador por tipo
   - Shrinking manual o limitado
   - No hay integración con el sistema de tipos
*/
```

### Go: testing/quick (built-in limitado) + gopter

```go
// Go tiene testing/quick en la stdlib, pero es limitado
import "testing/quick"

// La función debe aceptar tipos generados automáticamente
func TestSortPreservesLength(t *testing.T) {
    f := func(v []int) bool {
        original := len(v)
        sort.Ints(v)
        return len(v) == original
    }
    if err := quick.Check(f, nil); err != nil {
        t.Error(err)
    }
}

// Limitaciones de testing/quick:
// - Solo tipos primitivos y structs simples
// - Sin shrinking
// - Sin control fino de generadores
// - Sin regressions guardadas

// gopter es la alternativa completa (como proptest):
import "github.com/leanovate/gopter"
import "github.com/leanovate/gopter/gen"

func TestSortWithGopter(t *testing.T) {
    properties := gopter.NewProperties(nil)

    properties.Property("sort preserves length", prop.ForAll(
        func(v []int) bool {
            original := len(v)
            sort.Ints(v)
            return len(v) == original
        },
        gen.SliceOf(gen.Int()),
    ))

    properties.TestingRun(t)
}
```

### Tabla comparativa

| Aspecto | C (theft) | Go (testing/quick) | Go (gopter) | Rust (proptest) |
|---------|-----------|-------------------|-------------|-----------------|
| **Built-in** | No | Sí (limitado) | No | No (crate) |
| **Generadores** | Manuales, C puro | Automáticos, limitados | Componibles | Componibles |
| **Shrinking** | Básico | No | Sí | Determinístico |
| **Tipos custom** | Manual alloc/free | Implementar `Generate` | `Gen` functions | `Strategy` trait |
| **Regressions** | No | No | No | Automáticas |
| **Integración tipos** | Débil | Media | Buena | Excelente |
| **Memory safety** | Manual | GC | GC | Ownership |
| **Regex strings** | No | No | No | Sí |

---

## 18. Historia y origen: de QuickCheck a hoy

```
Línea de tiempo del Property-Based Testing:

  1999  QuickCheck (Haskell)
        │  Koen Claessen y John Hughes
        │  Paper: "QuickCheck: A Lightweight Tool for Random Testing"
        │  Inventó: generación aleatoria + shrinking + propiedades
        │
  2006  ScalaCheck (Scala)
        │  Adaptación para JVM
        │
  2009  QuickCheck para Erlang (Quviq)
        │  Versión comercial con state machine testing
        │  Encontró bugs en Ericsson, Volvo, AUTOSAR
        │
  2014  Hypothesis (Python)
        │  David MacIver
        │  Innovación: shrinking basado en bytes, no en tipo
        │  Popularizó PBT fuera del mundo funcional
        │
  2015  quickcheck (Rust)
        │  Andrew Gallant (BurntSushi)
        │  Port directo del concepto de Haskell
        │
  2017  proptest (Rust)
        │  Jason Lingle
        │  Inspirado por Hypothesis
        │  Shrinking determinístico, strategies componibles
        │
  2018+ Adopción generalizada
        │  PBT disponible en casi todos los lenguajes
        │  fast-check (JavaScript), jqwik (Java),
        │  FsCheck (F#/C#), Hedgehog (Haskell v2)
        │
  Hoy   Property testing es una técnica estándar
        en la industria, especialmente para:
        - Parsers y serialización
        - Protocolos de red
        - Bases de datos (CockroachDB, TiKV)
        - Criptografía
        - Sistemas distribuidos
```

### Impacto real

```
Bugs encontrados por PBT en proyectos reales:

  Volvo (2009):
    QuickCheck encontró 200+ bugs en el protocolo AUTOSAR
    que tests manuales de 3 años no habían encontrado

  Dropbox (2016):
    Hypothesis encontró un bug en la sincronización de archivos
    que causaba pérdida de datos en edge cases

  LevelDB (Google):
    PBT encontró data corruption bugs

  CockroachDB:
    Usa PBT extensivamente para verificar SQL correctness

  TiKV (PingCAP):
    Usa PBT para verificar su motor de almacenamiento Raft

  Rust compiler:
    proptest se usa para probar partes del compilador
```

---

## 19. Errores conceptuales comunes

### Error 1: "PBT reemplaza los tests de ejemplo"

```
✗ INCORRECTO: "Ya no necesito test_sort([3,1,4]) == [1,3,4]"

✓ CORRECTO: PBT COMPLEMENTA los tests de ejemplo

  Tests de ejemplo:
    - Documentan el comportamiento esperado
    - Son legibles (el reader ve input → output)
    - Verifican valores específicos conocidos
    - Son rápidos de escribir y ejecutar

  Property tests:
    - Exploran el espacio de inputs
    - Encuentran edge cases inesperados
    - Verifican propiedades generales
    - Son más lentos de ejecutar

  Ambos son valiosos. Usa ambos.
```

### Error 2: "La propiedad debe verificar el resultado exacto"

```rust
// ✗ INCORRECTO: esto es un test de ejemplo disfrazado
fn prop_bad(v: Vec<i32>) -> bool {
    let sorted = sort(v.clone());
    sorted == expected_sort_result(v)
    // ¿De dónde sale expected_sort_result?
    // Si la implementas, estás reimplementando sort
}

// ✓ CORRECTO: verificar PROPIEDADES, no el resultado exacto
fn prop_good(v: Vec<i32>) -> bool {
    let mut sorted = v.clone();
    sorted.sort();
    // Propiedad 1: está ordenado
    sorted.windows(2).all(|w| w[0] <= w[1])
    // No necesito saber el resultado exacto
}
```

### Error 3: "Necesito generar inputs que cubran todos los edge cases"

```
✗ INCORRECTO: "Debo generar específicamente MAX, MIN, 0, etc."

✓ CORRECTO: Un buen framework SESGA automáticamente hacia edge cases

  proptest genera automáticamente:
    - 0, 1, -1
    - Valores cerca de MIN y MAX
    - Strings vacíos
    - Vectores vacíos y de longitud 1
    - Caracteres Unicode especiales

  No necesitas configurar esto — viene por defecto.
  Si necesitas MÁS control, defines estrategias custom (T02).
```

### Error 4: "Si mi property test pasa 1000 veces, mi código es correcto"

```
✗ INCORRECTO: PBT no es una PRUEBA de corrección

✓ CORRECTO: PBT aumenta la CONFIANZA

  Después de 1000 iteraciones:
    - Es MUY PROBABLE que las propiedades verificadas se cumplen
    - Pero no es certeza matemática
    - Hay inputs que no se generaron
    - Hay propiedades que no definiste

  Analogía:
    Si tiras una moneda 1000 veces y sale cara,
    ¿es una moneda de dos caras? Muy probable, pero no seguro.
    Con PBT es lo mismo pero la moneda tiene millones de lados.
```

### Error 5: "La propiedad 'no crashea' es inútil"

```
✗ INCORRECTO: "prop_no_crash no verifica nada útil"

✓ CORRECTO: Es la propiedad MÍNIMA pero ya descubre bugs reales

  Ejemplos de bugs que "no crash" descubre:
    - Integer overflow en inputs grandes
    - Index out of bounds con longitudes inesperadas
    - Division by zero con inputs que resultan en 0
    - Stack overflow con inputs profundamente anidados
    - Infinite loops con ciertos patrones de datos

  "No crash" es el primer paso. Luego adds propiedades más fuertes.
```

### Error 6: "Necesito muchos más casos para encontrar bugs"

```
✗ INCORRECTO: "Debo ejecutar 1_000_000 de casos"

✓ CORRECTO: 100-256 casos son suficientes para la MAYORÍA de bugs

  Razón: el sesgo hacia edge cases es más importante que la cantidad.
  100 inputs bien sesgados son mejor que 1_000_000 uniformes.

  proptest default: 256 casos
  quickcheck default: 100 casos

  Si crees que necesitas más, probablemente necesitas
  una estrategia de generación mejor, no más iteraciones.
```

---

## 20. Ejemplo completo: descubriendo bugs con properties

Vamos a construir un ejemplo donde los tests de ejemplo pasan pero un property test descubre un bug real.

### El código con un bug oculto

```rust
// src/lib.rs

/// Comprime una secuencia de caracteres usando Run-Length Encoding.
///
/// "AAABBC" → "3A2B1C"
pub fn rle_encode(input: &str) -> String {
    if input.is_empty() {
        return String::new();
    }

    let mut result = String::new();
    let mut chars = input.chars();
    let mut current = chars.next().unwrap();
    let mut count = 1;

    for ch in chars {
        if ch == current {
            count += 1;
        } else {
            result.push_str(&format!("{}{}", count, current));
            current = ch;
            count = 1;
        }
    }
    result.push_str(&format!("{}{}", count, current));

    result
}

/// Descomprime una secuencia RLE.
///
/// "3A2B1C" → "AAABBC"
pub fn rle_decode(input: &str) -> Result<String, String> {
    if input.is_empty() {
        return Ok(String::new());
    }

    let mut result = String::new();
    let mut chars = input.chars().peekable();

    while chars.peek().is_some() {
        // Leer el número
        let mut num_str = String::new();
        while let Some(&ch) = chars.peek() {
            if ch.is_ascii_digit() {
                num_str.push(ch);
                chars.next();
            } else {
                break;
            }
        }

        if num_str.is_empty() {
            return Err(format!("Expected number, found {:?}", chars.peek()));
        }

        let count: usize = num_str.parse()
            .map_err(|e| format!("Invalid number: {}", e))?;

        // Leer el carácter
        let ch = chars.next()
            .ok_or_else(|| "Expected character after number".to_string())?;

        for _ in 0..count {
            result.push(ch);
        }
    }

    Ok(result)
}
```

### Tests de ejemplo que PASAN

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // Todos estos tests pasan ✓
    #[test]
    fn test_encode_simple() {
        assert_eq!(rle_encode("AAABBC"), "3A2B1C");
    }

    #[test]
    fn test_encode_single() {
        assert_eq!(rle_encode("A"), "1A");
    }

    #[test]
    fn test_encode_empty() {
        assert_eq!(rle_encode(""), "");
    }

    #[test]
    fn test_encode_no_repeats() {
        assert_eq!(rle_encode("ABCD"), "1A1B1C1D");
    }

    #[test]
    fn test_decode_simple() {
        assert_eq!(rle_decode("3A2B1C").unwrap(), "AAABBC");
    }

    #[test]
    fn test_decode_empty() {
        assert_eq!(rle_decode("").unwrap(), "");
    }

    #[test]
    fn test_roundtrip_basic() {
        let input = "AAABBC";
        assert_eq!(rle_decode(&rle_encode(input)).unwrap(), input);
    }
}
```

Todos pasan. ¿El código es correcto? Veamos qué dice un property test:

### Property test que encuentra el bug

```rust
// En tests/property_tests.rs (o dentro de mod tests con proptest)

use proptest::prelude::*;

proptest! {
    #[test]
    fn prop_rle_roundtrip(s in "[A-Za-z]{0,50}") {
        // Propiedad: decode(encode(s)) == s
        let encoded = rle_encode(&s);
        let decoded = rle_decode(&encoded).unwrap();
        prop_assert_eq!(decoded, s);
    }
}
```

### El fallo

```
$ cargo test prop_rle_roundtrip

test prop_rle_roundtrip ... FAILED

thread 'prop_rle_roundtrip' panicked at 'assertion failed:
  decoded == s
  left:  "111A"
  right: "1A"

  Failing input: s = "1A"
```

### ¡El bug!

El input `"1A"` rompe el roundtrip:

```
  encode("1A") = "11A"    ← "1 repetición de '1', 1 repetición de 'A'"
  decode("11A") = "AAAAAAAAAAA"  ← "11 repeticiones de 'A'"
  "AAAAAAAAAAA" ≠ "1A"   ← ¡FALLO!

El bug: cuando el input contiene DÍGITOS como caracteres,
encode produce output ambiguo:
  "11A" podría ser "1 '1' + 1 'A'" o "11 'A'"
  decode lo interpreta como "11 'A'" → bug!
```

El programador probó con letras (A, B, C) pero nunca con dígitos como parte del input. El property test generó `"1A"` automáticamente y el shrinking lo redujo al caso mínimo.

### La corrección

```rust
// Opciones para arreglar:
// 1. Usar un delimitador: "1:A2:B" en vez de "1A2B"
// 2. Escapar dígitos en el input
// 3. Documentar que el input no puede contener dígitos
// 4. Usar formato diferente: largo fijo para el conteo

// Opción 1: con delimitador
pub fn rle_encode_v2(input: &str) -> String {
    if input.is_empty() { return String::new(); }

    let mut result = String::new();
    let mut chars = input.chars();
    let mut current = chars.next().unwrap();
    let mut count: usize = 1;

    for ch in chars {
        if ch == current {
            count += 1;
        } else {
            if !result.is_empty() { result.push(','); }
            result.push_str(&format!("{}:{}", count, current));
            current = ch;
            count = 1;
        }
    }
    if !result.is_empty() { result.push(','); }
    result.push_str(&format!("{}:{}", count, current));

    result
}
// "1A" → "1:1,1:A" → decode → "1A" ✓
```

### Ahora el property test pasa

```rust
proptest! {
    #[test]
    fn prop_rle_v2_roundtrip(s in ".{0,50}") {
        // Ahora funciona con CUALQUIER carácter
        let encoded = rle_encode_v2(&s);
        let decoded = rle_decode_v2(&encoded).unwrap();
        prop_assert_eq!(decoded, s);
    }
}
```

```
$ cargo test prop_rle_v2_roundtrip
test prop_rle_v2_roundtrip ... ok (256 cases tested)
```

---

## 21. Programa de práctica

Implementa una biblioteca de **operaciones con fracciones** (`Fraction`) y descubre bugs usando property testing.

### Requisitos de la biblioteca

```rust
// src/lib.rs

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Fraction {
    numerator: i64,
    denominator: i64,
}

impl Fraction {
    /// Crea una fracción simplificada. Panics si denominator == 0.
    pub fn new(num: i64, den: i64) -> Self { todo!() }

    pub fn numerator(&self) -> i64 { todo!() }
    pub fn denominator(&self) -> i64 { todo!() }

    /// Suma dos fracciones.
    pub fn add(&self, other: &Fraction) -> Fraction { todo!() }

    /// Multiplica dos fracciones.
    pub fn multiply(&self, other: &Fraction) -> Fraction { todo!() }

    /// Retorna la fracción inversa (a/b → b/a).
    pub fn inverse(&self) -> Result<Fraction, String> { todo!() }

    /// Convierte a f64.
    pub fn to_f64(&self) -> f64 { todo!() }

    /// Parsea "3/4" a Fraction.
    pub fn parse(s: &str) -> Result<Fraction, String> { todo!() }

    /// Formatea como "3/4".
    pub fn format(&self) -> String { todo!() }
}
```

### Requisitos de los tests

Escribe **tests de ejemplo** (example-based) para las funciones básicas (mínimo 8 tests).

Luego escribe **property tests** para verificar al menos estas 10 propiedades:

```
Propiedades a verificar:

  1. ROUND-TRIP: parse(format(f)) == f
  2. ROUND-TRIP: format(parse(s)) == s (para strings válidos)
  3. ALGEBRAICA: add es conmutativa — a + b == b + a
  4. ALGEBRAICA: add es asociativa — (a+b)+c == a+(b+c)
  5. ALGEBRAICA: multiply es conmutativa — a * b == b * a
  6. ALGEBRAICA: identidad aditiva — a + 0/1 == a
  7. ALGEBRAICA: identidad multiplicativa — a * 1/1 == a
  8. INVERSA: a * inverse(a) == 1/1 (si a ≠ 0)
  9. INVARIANTE: denominador siempre positivo después de new()
  10. INVARIANTE: la fracción siempre está simplificada (gcd == 1)
```

### Bugs intencionales a introducir (elige 2)

Implementa la biblioteca con **2 bugs intencionales** de esta lista. Los property tests deben encontrarlos:

- Bug A: No simplificar cuando numerator es negativo
- Bug B: Overflow en add cuando los denominadores son grandes
- Bug C: inverse de fracción negativa pierde el signo
- Bug D: parse no maneja espacios ("3 / 4")
- Bug E: denominador 0 no verificado en add/multiply

Documenta cuáles bugs introdujiste y qué propiedad los detecta.

---

## 22. Ejercicios

### Ejercicio 1: Identificar propiedades

Para cada función, lista al menos 3 propiedades que un property test podría verificar:

**a)** `fn to_uppercase(s: &str) -> String`

**b)** `fn deduplicate(v: &mut Vec<i32>)`

**c)** `fn binary_search(sorted: &[i32], target: i32) -> Option<usize>`

**d)** `fn matrix_multiply(a: &Matrix, b: &Matrix) -> Matrix`

**e)** `fn compress(data: &[u8]) -> Vec<u8>`

---

### Ejercicio 2: Propiedad o ejemplo

Para cada escenario, decide si es mejor un test de ejemplo o un property test, y explica por qué:

**a)** Verificar que `parse_date("2024-01-15")` retorna la fecha correcta.

**b)** Verificar que `sort` produce un resultado ordenado para cualquier input.

**c)** Verificar que el endpoint `/api/health` retorna status 200.

**d)** Verificar que `json_encode` seguido de `json_decode` produce el original.

**e)** Verificar que la función de hash produce el mismo resultado para el mismo input.

**f)** Verificar que `trim` es idempotente.

---

### Ejercicio 3: Escribir propiedades para un HashMap

Dado un HashMap<String, i32> con operaciones `insert`, `get`, `remove`, `contains_key`, `len`:

**a)** Escribe la propiedad "insert seguido de get retorna el valor insertado".

**b)** Escribe la propiedad "remove seguido de contains_key retorna false".

**c)** Escribe la propiedad "insert aumenta len en 1 si la clave es nueva".

**d)** Escribe la propiedad "insert no cambia len si la clave ya existe".

**e)** Escribe una propiedad que use un modelo oracle (comparar con `std::collections::HashMap`).

---

### Ejercicio 4: Análisis de un bug report

Un property test reporta:

```
Test prop_encode_decode_roundtrip FAILED
  Minimal failing input: s = "\0"
  encode("\0") = "0"
  decode("0") = Err("Expected character after number")
```

**a)** Explica el bug.

**b)** ¿Por qué un test de ejemplo probablemente no lo habría encontrado?

**c)** ¿Qué propiedad se violó?

**d)** Propón al menos 2 formas de corregir el bug.

**e)** ¿Qué propiedad adicional agregarías para prevenir bugs similares?

---

## Navegación

- **Anterior**: [S02/T04 - cargo test flags](../../S02_Integration_Tests_y_Doc_Tests/T04_Cargo_test_flags/README.md)
- **Siguiente**: [T02 - proptest](../T02_Proptest/README.md)

---

> **Sección 3: Property-Based Testing** — Tópico 1 de 4 completado
>
> - T01: Concepto — generación aleatoria, propiedades invariantes, shrinking, tipos de propiedades ✓
> - T02: proptest (siguiente)
> - T03: Diseñar propiedades
> - T04: proptest vs quickcheck
