# T02 - proptest: el framework de Property-Based Testing en Rust

## Índice

1. [Qué es proptest y por qué usarlo](#1-qué-es-proptest-y-por-qué-usarlo)
2. [Instalación y configuración inicial](#2-instalación-y-configuración-inicial)
3. [La macro proptest!: anatomía completa](#3-la-macro-proptest-anatomía-completa)
4. [any::\<T\>() — generación automática de tipos](#4-anyt--generación-automática-de-tipos)
5. [Estrategias built-in para tipos primitivos](#5-estrategias-built-in-para-tipos-primitivos)
6. [Rangos y filtros sobre estrategias](#6-rangos-y-filtros-sobre-estrategias)
7. [Estrategias para colecciones](#7-estrategias-para-colecciones)
8. [Estrategias para String y texto](#8-estrategias-para-string-y-texto)
9. [Regex estrategias: generar strings desde patrones](#9-regex-estrategias-generar-strings-desde-patrones)
10. [Estrategias custom con prop_compose!](#10-estrategias-custom-con-prop_compose)
11. [Combinadores de estrategias](#11-combinadores-de-estrategias)
12. [prop_oneof! y Union de estrategias](#12-prop_oneof-y-union-de-estrategias)
13. [Generar enums con estrategias](#13-generar-enums-con-estrategias)
14. [Implementar Arbitrary para tipos propios](#14-implementar-arbitrary-para-tipos-propios)
15. [ProptestConfig: configuración del runner](#15-proptestconfig-configuración-del-runner)
16. [Archivos de regresión (.proptest-regressions)](#16-archivos-de-regresión-proptest-regressions)
17. [Shrinking en proptest: cómo funciona internamente](#17-shrinking-en-proptest-cómo-funciona-internamente)
18. [prop_assert!, prop_assert_eq!, prop_assume!](#18-prop_assert-prop_assert_eq-prop_assume)
19. [Estrategias dependientes: prop_flat_map](#19-estrategias-dependientes-prop_flat_map)
20. [Testing de máquinas de estado con proptest](#20-testing-de-máquinas-de-estado-con-proptest)
21. [Patrones avanzados y trucos](#21-patrones-avanzados-y-trucos)
22. [Comparación con C y Go](#22-comparación-con-c-y-go)
23. [Errores comunes con proptest](#23-errores-comunes-con-proptest)
24. [Ejemplo completo: validador de expresiones matemáticas](#24-ejemplo-completo-validador-de-expresiones-matemáticas)
25. [Programa de práctica](#25-programa-de-práctica)
26. [Ejercicios](#26-ejercicios)

---

## 1. Qué es proptest y por qué usarlo

`proptest` es el framework dominante de property-based testing en Rust. Creado por Jason Lingle (inspirado en Hypothesis para Python), proptest genera inputs aleatorios, verifica propiedades y reduce automáticamente los fallos al caso mínimo reproducible.

### ¿Por qué proptest sobre otros frameworks?

```
┌─────────────────────────────────────────────────────────────────┐
│                    PROPTEST EN EL ECOSISTEMA                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  proptest (dominante)                                           │
│  ├── Shrinking determinístico (basado en valor, no en historia) │
│  ├── API rica y ergonómica                                      │
│  ├── Generadores de regex                                       │
│  ├── Composición funcional de estrategias                       │
│  ├── Archivos de regresión automáticos                          │
│  ├── Mantenido activamente (500K+ descargas/mes)                │
│  └── Inspirado en Hypothesis (Python)                           │
│                                                                 │
│  quickcheck (legado)                                            │
│  ├── Port directo de QuickCheck (Haskell)                       │
│  ├── API más simple pero menos potente                          │
│  ├── Shrinking no determinístico                                │
│  └── Menor actividad de desarrollo                              │
│                                                                 │
│  bolero (fuzzing + PBT)                                         │
│  ├── Unifica fuzzing y property testing                         │
│  ├── Backends: libfuzzer, afl, proptest                         │
│  └── Nicho más especializado                                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Características clave de proptest

| Característica | Descripción |
|---|---|
| **Estrategias** | Objetos que saben generar valores aleatorios de un tipo |
| **Shrinking determinístico** | Reduce por estructura del valor, no por historial de ejecución |
| **Composición** | Las estrategias se componen como iteradores (map, filter, flat_map) |
| **Regex generation** | Genera strings que matchean un regex dado |
| **Regresión automática** | Guarda inputs que fallaron para re-testearlos siempre |
| **Configuración flexible** | Controlar iteraciones, semilla, timeout por test |

---

## 2. Instalación y configuración inicial

### Dependencia básica

```toml
# Cargo.toml
[dev-dependencies]
proptest = "1"
```

`proptest` solo se necesita como `dev-dependency` porque los property tests solo se ejecutan en modo test.

### Importación en tests

```rust
// En un módulo de tests o archivo de integración
use proptest::prelude::*;
```

El prelude importa todo lo necesario:

```rust
// Lo que contiene proptest::prelude::*
//
// Macros:
//   proptest!          — macro principal para definir property tests
//   prop_compose!      — crear estrategias custom
//   prop_oneof!        — unión de estrategias
//   prop_assert!       — assert dentro de proptest
//   prop_assert_eq!    — assert_eq dentro de proptest
//   prop_assert_ne!    — assert_ne dentro de proptest
//   prop_assume!       — filtrar inputs inválidos
//
// Traits:
//   Strategy           — trait base de todas las estrategias
//   Arbitrary          — trait para tipos con estrategia por defecto
//
// Funciones:
//   any::<T>()         — estrategia por defecto para T
//
// Tipos:
//   ProptestConfig     — configuración del runner
//   TestCaseError      — error de un caso de test
```

### Estructura mínima de proyecto

```
mi_proyecto/
├── Cargo.toml          # proptest en [dev-dependencies]
├── src/
│   └── lib.rs          # código a testear
└── tests/
    └── properties.rs   # property tests de integración
```

### Versioning y compatibilidad

```toml
# Pinear versión mayor es suficiente
proptest = "1"         # ≥1.0.0, <2.0.0

# Si necesitas features específicas
proptest = { version = "1", features = ["std"] }

# Features disponibles:
# "std"            — soporte de la librería estándar (default)
# "alloc"          — solo alloc, sin std
# "unstable"       — APIs experimentales
# "hardware-rng"   — usar RNG por hardware para semillas
# "bit-set"        — estrategias para BitSet
# "regex-syntax"   — estrategias basadas en regex (default)
```

---

## 3. La macro proptest!: anatomía completa

La macro `proptest!` es el punto de entrada para escribir property tests. Genera funciones `#[test]` estándar de Rust con toda la maquinaria de generación, ejecución y shrinking.

### Forma básica

```rust
use proptest::prelude::*;

proptest! {
    #[test]
    fn test_name(input in strategy) {
        // cuerpo: verificar propiedades sobre `input`
        // usar prop_assert! en lugar de assert!
    }
}
```

### Expansión de la macro

```
proptest! {                          Se expande a:
    #[test]                          ┌─────────────────────────────────┐
    fn mi_test(x in 0..100i32) {     │ #[test]                        │
        prop_assert!(x >= 0);        │ fn mi_test() {                 │
    }                                │     let config = ProptestConfig │
}                                    │         ::default();            │
                                     │     let mut runner =            │
                                     │         TestRunner::new(config);│
                                     │     runner.run(                 │
                                     │         &(0..100i32),           │
                                     │         |x| {                  │
                                     │             prop_assert!(x >= 0)│
                                     │                 ?;              │
                                     │             Ok(())             │
                                     │         }                      │
                                     │     ).unwrap();                │
                                     │ }                              │
                                     └─────────────────────────────────┘
```

### Múltiples inputs

```rust
proptest! {
    #[test]
    fn suma_conmutativa(a in any::<i32>(), b in any::<i32>()) {
        // a y b se generan independientemente
        prop_assert_eq!(a.wrapping_add(b), b.wrapping_add(a));
    }
}
```

### Múltiples tests en un bloque

```rust
proptest! {
    #[test]
    fn test_uno(x in 0..100i32) {
        prop_assert!(x < 100);
    }

    #[test]
    fn test_dos(s in "[a-z]+") {
        prop_assert!(!s.is_empty());
    }

    #[test]
    fn test_tres(v in prop::collection::vec(any::<u8>(), 0..50)) {
        prop_assert!(v.len() < 50);
    }
}
```

### Con configuración custom

```rust
proptest! {
    // Configuración aplicada a todos los tests del bloque
    #![proptest_config(ProptestConfig::with_cases(10_000))]

    #[test]
    fn test_intensivo(x in any::<i64>()) {
        // Se ejecuta 10,000 veces en lugar de las 256 por defecto
        prop_assert_eq!(x.wrapping_mul(1), x);
    }
}
```

### Forma con TestRunner (manual, sin macro)

```rust
#[test]
fn test_manual() {
    let mut config = ProptestConfig::default();
    config.cases = 500;
    
    let mut runner = proptest::test_runner::TestRunner::new(config);
    
    runner.run(&(0..100i32), |x| {
        prop_assert!(x >= 0)?;
        prop_assert!(x < 100)?;
        Ok(())
    }).unwrap();
}
```

La forma manual es útil cuando necesitas más control, pero la macro `proptest!` es preferible en el 99% de los casos.

### Lo que NO se puede hacer dentro de proptest!

```rust
proptest! {
    #[test]
    fn ejemplo(x in any::<i32>()) {
        // ❌ NO usar assert! (no integra con shrinking)
        // assert!(x > 0);
        
        // ✅ Usar prop_assert!
        prop_assert!(x != i32::MIN || x == i32::MIN); // tautología de ejemplo
        
        // ❌ NO usar panic! directamente
        // if x == 42 { panic!("No me gusta 42"); }
        
        // ✅ Usar prop_assert! con condición
        // prop_assert_ne!(x, 42); // esto fallaría y mostraría shrinking
        
        // ❌ NO retornar valores arbitrarios
        // return x + 1;
        
        // ✅ El cuerpo debe retornar () implícitamente o Ok(())
    }
}
```

---

## 4. any::\<T\>() — generación automática de tipos

`any::<T>()` retorna la estrategia "por defecto" para generar valores de tipo `T`. Funciona con cualquier tipo que implemente el trait `Arbitrary`.

### Tipos con soporte built-in

```rust
use proptest::prelude::*;

proptest! {
    #[test]
    fn demo_any(
        // Enteros
        b in any::<bool>(),
        i8_val in any::<i8>(),
        u32_val in any::<u32>(),
        i64_val in any::<i64>(),
        u128_val in any::<u128>(),
        usize_val in any::<usize>(),
        
        // Flotantes
        f32_val in any::<f32>(),
        f64_val in any::<f64>(),
        
        // Char
        c in any::<char>(),
        
        // String
        s in any::<String>(),
        
        // Option y Result
        opt in any::<Option<i32>>(),
        res in any::<Result<u8, String>>(),
        
        // Tuplas (hasta 12 elementos)
        pair in any::<(i32, bool)>(),
        triple in any::<(u8, String, f64)>(),
        
        // Arrays
        arr in any::<[u8; 4]>(),
        
        // Vec (tamaño 0..100 por defecto)
        v in any::<Vec<i32>>(),
    ) {
        // Todos los valores son válidos para su tipo
        let _ = (b, i8_val, u32_val, i64_val, u128_val, usize_val);
        let _ = (f32_val, f64_val, c, s, opt, res, pair, triple, arr, v);
    }
}
```

### Qué genera any::\<T\>() para cada tipo

```
┌──────────────────┬────────────────────────────────────────────┐
│ Tipo             │ Rango de generación                       │
├──────────────────┼────────────────────────────────────────────┤
│ bool             │ true, false                                │
│ i8               │ -128..=127                                 │
│ u8               │ 0..=255                                    │
│ i32              │ i32::MIN..=i32::MAX                        │
│ u64              │ 0..=u64::MAX                               │
│ f32              │ Todo f32, INCLUYENDO NaN, ±Inf, ±0, sub-   │
│                  │ normales. Sesgo hacia especiales.           │
│ f64              │ Igual que f32 pero para f64                 │
│ char             │ Todo codepoint Unicode válido               │
│ String           │ Chars Unicode, longitud 0..=100             │
│ Vec<T>           │ Elementos any::<T>(), longitud 0..=100      │
│ Option<T>        │ None con ~50%, Some(any::<T>()) con ~50%    │
│ Result<T, E>     │ Ok(any::<T>()) ~50%, Err(any::<E>()) ~50%  │
│ (A, B)           │ (any::<A>(), any::<B>()) independientes     │
│ [T; N]           │ N elementos any::<T>()                      │
│ Box<T>           │ Box::new(any::<T>())                        │
│ Rc<T>, Arc<T>    │ Wrapping any::<T>()                         │
│ HashMap<K,V>     │ Pares any::<(K,V)>(), tamaño 0..=100       │
│ HashSet<T>       │ Elementos any::<T>(), tamaño 0..=100        │
│ BTreeMap<K,V>    │ Igual que HashMap                           │
│ BTreeSet<T>      │ Igual que HashSet                           │
└──────────────────┴────────────────────────────────────────────┘
```

### Cuidado con flotantes

```rust
proptest! {
    #[test]
    fn flotantes_sorprendentes(x in any::<f64>()) {
        // ❌ Esto FALLARÁ: any::<f64>() genera NaN
        // prop_assert_eq!(x, x); // NaN != NaN
        
        // ✅ Si necesitas evitar NaN:
        prop_assume!(!x.is_nan());
        prop_assert_eq!(x, x);
    }
}

proptest! {
    #[test]
    fn flotantes_sin_nan(x in proptest::num::f64::NORMAL) {
        // NORMAL excluye NaN, Inf, subnormales
        prop_assert_eq!(x, x);
        prop_assert!(x.is_finite());
        prop_assert!(x.is_normal() || x == 0.0);
    }
}
```

### Subconjuntos de flotantes disponibles

```rust
use proptest::num::f64;

// Subconjuntos de f64 (lo mismo existe para f32)
let _ = f64::ANY;        // Todos (NaN, Inf, subnormales)
let _ = f64::NORMAL;     // Solo normales y ±0
let _ = f64::POSITIVE;   // > 0 (normales)
let _ = f64::NEGATIVE;   // < 0 (normales)
let _ = f64::ZERO;       // Solo 0.0 y -0.0
```

### any::\<T\>() no es magia: requiere Arbitrary

```rust
// Este struct NO funciona con any::<T>():
struct MiTipo {
    valor: i32,
}

// ❌ Error de compilación:
// proptest! {
//     #[test]
//     fn test_mi_tipo(x in any::<MiTipo>()) { ... }
// }
// error: `MiTipo` doesn't implement `Arbitrary`

// ✅ Solución 1: crear una estrategia manual
fn mi_tipo_strategy() -> impl Strategy<Value = MiTipo> {
    any::<i32>().prop_map(|valor| MiTipo { valor })
}

// ✅ Solución 2: implementar Arbitrary (ver sección 14)
```

---

## 5. Estrategias built-in para tipos primitivos

Más allá de `any::<T>()`, proptest ofrece estrategias especializadas que dan control fino sobre los valores generados.

### Rangos como estrategias

En Rust, los rangos implementan `Strategy` automáticamente:

```rust
proptest! {
    #[test]
    fn demo_rangos(
        // Range exclusivo (lo más común)
        a in 0..100i32,
        
        // Range inclusivo
        b in 0..=99i32,           // equivalente a 0..100 para enteros
        
        // Range desde
        c in 50i32..,             // 50 hasta i32::MAX
        
        // Valor exacto (Just strategy)
        d in Just(42i32),         // siempre 42
    ) {
        prop_assert!(a >= 0 && a < 100);
        prop_assert!(b >= 0 && b <= 99);
        prop_assert!(c >= 50);
        prop_assert_eq!(d, 42);
    }
}
```

### Tabla de estrategias para tipos numéricos

```
┌────────────────────────────┬──────────────────────────────────────┐
│ Estrategia                 │ Genera                               │
├────────────────────────────┼──────────────────────────────────────┤
│ any::<i32>()               │ Todo el rango de i32                 │
│ 0..100i32                  │ 0 ≤ x < 100                         │
│ 0..=99i32                  │ 0 ≤ x ≤ 99                          │
│ -10..10i32                 │ -10 ≤ x < 10                        │
│ Just(42i32)                │ Siempre 42                           │
│ prop_oneof![1..5, 90..100] │ 1..5 O 90..100 (50% cada uno)       │
│ (0..10i32, 0..10i32)       │ Tupla de dos valores independientes  │
└────────────────────────────┴──────────────────────────────────────┘
```

### Estrategias para bool

```rust
proptest! {
    #[test]
    fn demo_bool(
        b in any::<bool>(),       // true o false con 50% cada uno
        t in Just(true),          // siempre true
    ) {
        let _ = (b, t);
    }
}
```

### Estrategias para char

```rust
use proptest::char;

proptest! {
    #[test]
    fn demo_char(
        // Cualquier char Unicode válido
        c1 in any::<char>(),
        
        // Solo ASCII
        c2 in proptest::char::range('a', 'z'),
        
        // Rangos múltiples
        c3 in proptest::char::ranges(
            vec![('a'..='z').into(), ('A'..='Z').into(), ('0'..='9').into()].into()
        ),
    ) {
        prop_assert!(c2.is_ascii_lowercase());
        prop_assert!(c3.is_alphanumeric());
        let _ = c1;
    }
}
```

---

## 6. Rangos y filtros sobre estrategias

Las estrategias se pueden transformar y filtrar con métodos similares a los de iteradores.

### prop_map: transformar valores generados

```rust
use proptest::prelude::*;

proptest! {
    #[test]
    fn numeros_pares(n in (0..50i32).prop_map(|x| x * 2)) {
        prop_assert!(n % 2 == 0);
        prop_assert!(n >= 0 && n < 100);
    }
}

// Generar structs a partir de componentes
#[derive(Debug, PartialEq)]
struct Point {
    x: f64,
    y: f64,
}

proptest! {
    #[test]
    fn test_points(
        p in (proptest::num::f64::NORMAL, proptest::num::f64::NORMAL)
            .prop_map(|(x, y)| Point { x, y })
    ) {
        // p es un Point válido con coordenadas finitas
        prop_assert!(p.x.is_finite());
        prop_assert!(p.y.is_finite());
    }
}
```

### prop_filter: descartar valores inválidos

```rust
proptest! {
    #[test]
    fn solo_impares(
        n in (0..100i32).prop_filter("debe ser impar", |x| x % 2 != 0)
    ) {
        prop_assert!(n % 2 != 0);
    }
}
```

**Cuidado con filtros demasiado restrictivos:**

```rust
// ❌ MALO: filtra el 99.99% de los valores → MUY lento
proptest! {
    #[test]
    fn test_lento(
        n in any::<i32>().prop_filter("primo", |x| es_primo(*x))
    ) {
        // ...
    }
}

// ✅ MEJOR: generar directamente desde un dominio más acotado
proptest! {
    #[test]
    fn test_rapido(
        idx in 0..1000usize,
    ) {
        let primos = vec![2, 3, 5, 7, 11, 13, /* ... */];
        let n = primos[idx % primos.len()];
        prop_assert!(es_primo(n));
    }
}
```

> **Regla práctica**: si un `prop_filter` descarta más del 50% de los valores generados, proptest emitirá un warning. Si descarta más del ~90%, el test puede fallar con `too many rejections`.

### prop_filter_map: filtrar y transformar en un paso

```rust
proptest! {
    #[test]
    fn solo_ascii_mayusculas(
        c in any::<char>().prop_filter_map(
            "debe ser ASCII mayúscula",
            |c| {
                if c.is_ascii_uppercase() {
                    Some(c)
                } else {
                    None
                }
            }
        )
    ) {
        prop_assert!(c.is_ascii_uppercase());
    }
}
```

### prop_map encadenados

```rust
fn email_strategy() -> impl Strategy<Value = String> {
    ("[a-z]{3,10}", "[a-z]{2,5}")
        .prop_map(|(user, domain)| format!("{}@{}.com", user, domain))
}

proptest! {
    #[test]
    fn test_emails(email in email_strategy()) {
        prop_assert!(email.contains('@'));
        prop_assert!(email.ends_with(".com"));
    }
}
```

---

## 7. Estrategias para colecciones

proptest proporciona estrategias dedicadas para generar `Vec`, `HashMap`, `HashSet`, `BTreeMap`, `BTreeSet`, `VecDeque`, `LinkedList` y `BinaryHeap`.

### Vec con control de tamaño

```rust
use proptest::collection;

proptest! {
    #[test]
    fn test_vec(
        // Vec de 0 a 100 elementos (default con any)
        v1 in any::<Vec<i32>>(),
        
        // Vec con tamaño exacto controlado
        v2 in collection::vec(any::<i32>(), 0..50),
        
        // Vec de exactamente 10 elementos
        v3 in collection::vec(1..100i32, 10),
        
        // Vec de 1 a 5 strings
        v4 in collection::vec("[a-z]+", 1..=5),
        
        // Vec vacío
        v5 in Just(Vec::<i32>::new()),
    ) {
        prop_assert!(v2.len() < 50);
        prop_assert_eq!(v3.len(), 10);
        prop_assert!(!v4.is_empty() && v4.len() <= 5);
        prop_assert!(v5.is_empty());
        let _ = v1;
    }
}
```

### HashMap y HashSet

```rust
use proptest::collection;
use std::collections::{HashMap, HashSet};

proptest! {
    #[test]
    fn test_maps(
        // HashMap con 0 a 20 entradas
        map in collection::hash_map("[a-z]{1,5}", 0..100i32, 0..20),
        
        // HashSet de 5 a 10 elementos
        set in collection::hash_set(1..1000u32, 5..=10),
    ) {
        prop_assert!(map.len() <= 20);
        prop_assert!(set.len() >= 5 && set.len() <= 10);
        
        for (k, v) in &map {
            prop_assert!(!k.is_empty());
            prop_assert!(*v >= 0 && *v < 100);
        }
    }
}
```

### BTreeMap y BTreeSet

```rust
use proptest::collection;
use std::collections::BTreeMap;

proptest! {
    #[test]
    fn test_btree(
        map in collection::btree_map(0..100i32, any::<String>(), 1..10),
    ) {
        // BTreeMap siempre está ordenado por clave
        let keys: Vec<_> = map.keys().collect();
        for window in keys.windows(2) {
            prop_assert!(window[0] < window[1]);
        }
    }
}
```

### VecDeque y LinkedList

```rust
use proptest::collection;
use std::collections::VecDeque;

proptest! {
    #[test]
    fn test_vecdeque(
        deque in collection::vec_deque(any::<u8>(), 0..20),
    ) {
        // VecDeque soporta push_front y push_back eficientemente
        prop_assert!(deque.len() < 20);
    }
}
```

### Composición: Vec de structs

```rust
#[derive(Debug, Clone)]
struct User {
    name: String,
    age: u8,
}

fn user_strategy() -> impl Strategy<Value = User> {
    ("[A-Z][a-z]{2,10}", 18..=100u8)
        .prop_map(|(name, age)| User { name, age })
}

proptest! {
    #[test]
    fn test_lista_usuarios(
        users in collection::vec(user_strategy(), 1..20),
    ) {
        prop_assert!(!users.is_empty());
        for user in &users {
            prop_assert!(user.age >= 18);
            prop_assert!(user.name.len() >= 3);
        }
    }
}
```

---

## 8. Estrategias para String y texto

La generación de strings es uno de los puntos fuertes de proptest, con estrategias que van desde "cualquier string Unicode" hasta "strings que matchean un regex".

### Estrategias de String básicas

```rust
proptest! {
    #[test]
    fn test_strings(
        // Cualquier String Unicode (puede incluir emojis, CJK, etc.)
        s1 in any::<String>(),
        
        // String con rango de longitud (en bytes)
        s2 in ".{1,20}",         // regex: 1 a 20 chars cualesquiera
        
        // Solo ASCII
        s3 in "[[:ascii:]]{0,50}",
        
        // Solo letras minúsculas
        s4 in "[a-z]{5,10}",
        
        // Alfanumérico
        s5 in "[a-zA-Z0-9]{1,30}",
    ) {
        prop_assert!(s4.chars().all(|c| c.is_ascii_lowercase()));
        prop_assert!(s4.len() >= 5);
        let _ = (s1, s2, s3, s5);
    }
}
```

### Diferencia entre longitud de bytes y chars

```rust
proptest! {
    #[test]
    fn unicode_vs_ascii(s in any::<String>()) {
        // s.len() cuenta BYTES, no caracteres
        // Un emoji puede ser 4 bytes pero 1 char
        let byte_len = s.len();
        let char_len = s.chars().count();
        
        // Siempre se cumple:
        prop_assert!(byte_len >= char_len);
    }
}
```

### Control preciso de strings

```rust
use proptest::string;

proptest! {
    #[test]
    fn string_controlada(
        // String de exactamente 10 chars ASCII lowercase
        s in "[a-z]{10}",
        
        // String numérica (simular IDs)
        id in "[0-9]{8}",
        
        // String con formato específico
        code in "[A-Z]{2}-[0-9]{4}",
    ) {
        prop_assert_eq!(s.len(), 10);
        prop_assert_eq!(id.len(), 8);
        
        // code tiene formato "XX-1234"
        prop_assert!(code.contains('-'));
        let parts: Vec<&str> = code.split('-').collect();
        prop_assert_eq!(parts.len(), 2);
        prop_assert_eq!(parts[0].len(), 2);
        prop_assert_eq!(parts[1].len(), 4);
    }
}
```

### Bytes y Vec\<u8\>

```rust
use proptest::collection;

proptest! {
    #[test]
    fn test_bytes(
        // Vec<u8> genérico
        bytes in collection::vec(any::<u8>(), 0..100),
        
        // Solo bytes ASCII
        ascii_bytes in collection::vec(0x20..0x7Fu8, 1..50),
        
        // Bytes que forman UTF-8 válido
        text_bytes in any::<String>().prop_map(|s| s.into_bytes()),
    ) {
        for &b in &ascii_bytes {
            prop_assert!(b >= 0x20 && b < 0x7F);
        }
        
        // text_bytes siempre es UTF-8 válido por construcción
        prop_assert!(String::from_utf8(text_bytes).is_ok());
        let _ = bytes;
    }
}
```

---

## 9. Regex estrategias: generar strings desde patrones

Una de las características más poderosas y distintivas de proptest es la capacidad de generar strings que matchean un regex dado. Esto permite generar datos con formato específico de manera declarativa.

### Sintaxis básica

```rust
// Un string literal entre comillas se interpreta como regex
proptest! {
    #[test]
    fn test_regex(s in "[a-zA-Z][a-zA-Z0-9_]{2,15}") {
        // s es un identificador válido: empieza con letra, 3-16 chars total
        prop_assert!(s.len() >= 3 && s.len() <= 16);
        prop_assert!(s.chars().next().unwrap().is_alphabetic());
    }
}
```

### Regex como estrategia explícita

```rust
use proptest::string::string_regex;

fn username_strategy() -> impl Strategy<Value = String> {
    string_regex("[a-z][a-z0-9]{2,11}").unwrap()
}

fn email_local_strategy() -> impl Strategy<Value = String> {
    string_regex("[a-z][a-z0-9._%+-]{0,19}").unwrap()
}
```

### Patrones comunes

```rust
proptest! {
    #[test]
    fn formatos_comunes(
        // Dirección IPv4 (simplificada)
        ipv4 in "[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}",
        
        // Fecha ISO (simplificada)
        date in "[12][0-9]{3}-[01][0-9]-[0-3][0-9]",
        
        // Código hexadecimal de color
        color in "#[0-9a-fA-F]{6}",
        
        // UUID v4 simplificado
        uuid in "[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}",
        
        // Ruta de archivo Unix
        path in "/[a-z]{1,8}(/[a-z]{1,8}){0,4}",
        
        // Versión semántica
        semver in "[0-9]{1,2}\\.[0-9]{1,2}\\.[0-9]{1,3}",
    ) {
        prop_assert!(ipv4.matches('.').count() == 3);
        prop_assert!(date.contains('-'));
        prop_assert!(color.starts_with('#'));
        prop_assert!(uuid.contains("-4")); // versión 4
        prop_assert!(path.starts_with('/'));
        prop_assert_eq!(semver.matches('.').count(), 2);
    }
}
```

### Limitaciones de las regex estrategias

```
┌─────────────────────────────────────────────────────────────────┐
│            LIMITACIONES DE REGEX STRATEGIES                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ✅ Soportado:                                                  │
│  ├── Clases de caracteres: [a-z], [0-9], \d, \w, \s            │
│  ├── Cuantificadores: *, +, ?, {n}, {n,m}                      │
│  ├── Alternación: a|b                                           │
│  ├── Grupos: (abc)                                              │
│  ├── Anclajes: ^, $ (ignorados, match completo)                 │
│  └── Clases POSIX: [:alpha:], [:digit:]                         │
│                                                                 │
│  ❌ No soportado:                                               │
│  ├── Lookahead/lookbehind: (?=...), (?<=...)                    │
│  ├── Backreferences: \1, \2                                     │
│  ├── Flags inline: (?i), (?m)                                   │
│  └── Regex con anchura cero compleja                            │
│                                                                 │
│  ⚠️  Precaución:                                                │
│  ├── Regex complejas son más lentas de generar                  │
│  ├── Cuantificadores sin límite (*, +) se limitan internamente  │
│  └── Regex con muchas alternativas pueden sesgar la distribución│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Combinar regex con otras estrategias

```rust
fn structured_log_strategy() -> impl Strategy<Value = String> {
    (
        prop_oneof!["INFO", "WARN", "ERROR", "DEBUG"],
        "[0-9]{4}-[01][0-9]-[0-3][0-9]",
        "[a-z_]{3,15}",
        ".{1,50}",
    )
        .prop_map(|(level, date, module, msg)| {
            format!("[{}] {} [{}] {}", level, date, module, msg)
        })
}

proptest! {
    #[test]
    fn test_parse_log(line in structured_log_strategy()) {
        prop_assert!(line.starts_with('['));
        // Verificar que nuestro parser puede manejar el log generado
    }
}
```

---

## 10. Estrategias custom con prop_compose!

`prop_compose!` es la forma idiomática de crear estrategias para tipos propios. Permite definir funciones generadoras que combinan múltiples estrategias.

### Sintaxis de prop_compose!

```rust
use proptest::prelude::*;

// Forma básica
prop_compose! {
    fn nombre_strategy()
        (param1 in strategy1, param2 in strategy2)
        -> TipoOutput
    {
        // Construir TipoOutput a partir de param1, param2
        TipoOutput { param1, param2 }
    }
}
```

### Ejemplo básico: Point

```rust
#[derive(Debug, Clone, PartialEq)]
struct Point {
    x: f64,
    y: f64,
}

prop_compose! {
    fn point_strategy()(
        x in -1000.0f64..1000.0,
        y in -1000.0f64..1000.0
    ) -> Point {
        Point { x, y }
    }
}

proptest! {
    #[test]
    fn test_point_distance(p in point_strategy()) {
        let dist = (p.x * p.x + p.y * p.y).sqrt();
        prop_assert!(dist >= 0.0);
    }
}
```

### Con parámetros de configuración

```rust
#[derive(Debug, Clone)]
struct Rectangle {
    width: u32,
    height: u32,
}

prop_compose! {
    // Los parámetros antes del primer paréntesis son argumentos
    // de la función generadora, NO valores generados
    fn rectangle_strategy(max_size: u32)(
        width in 1..=max_size,
        height in 1..=max_size
    ) -> Rectangle {
        Rectangle { width, height }
    }
}

proptest! {
    #[test]
    fn test_small_rect(r in rectangle_strategy(100)) {
        prop_assert!(r.width <= 100);
        prop_assert!(r.height <= 100);
    }

    #[test]
    fn test_large_rect(r in rectangle_strategy(10_000)) {
        prop_assert!(r.width <= 10_000);
    }
}
```

### Forma de dos etapas: dependencias entre valores

Cuando un valor generado depende de otro ya generado, se usa la forma de dos etapas:

```rust
#[derive(Debug)]
struct Matrix {
    rows: usize,
    cols: usize,
    data: Vec<f64>,
}

prop_compose! {
    fn matrix_strategy()(
        // Primera etapa: generar dimensiones
        rows in 1..10usize,
        cols in 1..10usize
    )(
        // Segunda etapa: usar dimensiones para generar datos
        // rows y cols están disponibles aquí
        data in proptest::collection::vec(
            proptest::num::f64::NORMAL,
            rows * cols  // ← depende de la primera etapa
        ),
        // Re-declarar los valores de la primera etapa
        rows in Just(rows),
        cols in Just(cols)
    ) -> Matrix {
        Matrix { rows, cols, data }
    }
}

proptest! {
    #[test]
    fn test_matrix(m in matrix_strategy()) {
        prop_assert_eq!(m.data.len(), m.rows * m.cols);
    }
}
```

```
Flujo de la forma de dos etapas:

  prop_compose! {
      fn strategy()(
          ┌──── Primera etapa ────┐
          │ Genera valores base   │
          │ rows = 3, cols = 4    │
          └───────┬───────────────┘
                  │ Estos valores se pasan
                  ▼ a la segunda etapa
          ┌──── Segunda etapa ────┐
          │ Puede usar valores de │
          │ la primera etapa      │
          │ data: vec![...; 3*4]  │
          │ rows: Just(3)  ← re-  │
          │ cols: Just(4)    bind  │
          └───────┬───────────────┘
                  │
                  ▼
          ┌──── Cuerpo ───────────┐
          │ Construye el valor     │
          │ final con todo        │
          └───────────────────────┘
      }
  }
```

### Ejemplo avanzado: árbol binario

```rust
#[derive(Debug, Clone)]
enum BinaryTree {
    Leaf(i32),
    Node {
        value: i32,
        left: Box<BinaryTree>,
        right: Box<BinaryTree>,
    },
}

// Para estructuras recursivas, usar prop_recursive
fn binary_tree_strategy() -> impl Strategy<Value = BinaryTree> {
    // Caso base: un Leaf
    let leaf = any::<i32>().prop_map(BinaryTree::Leaf);
    
    // Expansión recursiva
    leaf.prop_recursive(
        4,    // profundidad máxima
        64,   // nodos máximos
        2,    // items por recursión (left y right)
        |inner| {
            // inner es la estrategia recursiva (puede ser Leaf o Node)
            (any::<i32>(), inner.clone(), inner)
                .prop_map(|(value, left, right)| {
                    BinaryTree::Node {
                        value,
                        left: Box::new(left),
                        right: Box::new(right),
                    }
                })
        },
    )
}

proptest! {
    #[test]
    fn test_tree_depth(tree in binary_tree_strategy()) {
        fn depth(t: &BinaryTree) -> usize {
            match t {
                BinaryTree::Leaf(_) => 1,
                BinaryTree::Node { left, right, .. } => {
                    1 + depth(left).max(depth(right))
                }
            }
        }
        // La profundidad nunca excede el límite + 1 (caso base)
        prop_assert!(depth(&tree) <= 5);
    }
}
```

---

## 11. Combinadores de estrategias

Las estrategias en proptest implementan una interfaz funcional rica que permite composición sofisticada.

### Tabla de combinadores

```
┌──────────────────────┬──────────────────────────────────────────┐
│ Combinador           │ Descripción                              │
├──────────────────────┼──────────────────────────────────────────┤
│ .prop_map(f)         │ Transforma el valor generado             │
│ .prop_filter(msg, p) │ Descarta valores que no cumplen p        │
│ .prop_filter_map(m,f)│ Filtra y transforma en un paso           │
│ .prop_flat_map(f)    │ Genera una estrategia a partir del valor │
│ .prop_ind_flat_map(f)│ Como flat_map pero shrinking independiente│
│ .prop_shuffle()      │ Permuta aleatoriamente (para Vec)        │
│ .prop_recursive(d,s, │ Construye estructuras recursivas         │
│    i, f)             │                                          │
│ prop_oneof![a, b, c] │ Elige una estrategia al azar             │
│ Just(x)              │ Siempre retorna x                        │
│ (a, b, c)            │ Tupla: genera cada componente             │
└──────────────────────┴──────────────────────────────────────────┘
```

### Tuplas como estrategias

Las tuplas de estrategias son estrategias de tuplas:

```rust
proptest! {
    #[test]
    fn test_tuplas(
        // Tupla de 2
        pair in (any::<i32>(), any::<bool>()),
        
        // Tupla de 3
        triple in (0..10i32, "[a-z]+", any::<f64>()),
        
        // Tupla de estrategias transformadas con prop_map
        point in (0.0f64..100.0, 0.0f64..100.0)
            .prop_map(|(x, y)| (x, y, (x*x + y*y).sqrt())),
    ) {
        let (x, y, dist) = point;
        prop_assert!(dist >= 0.0);
        let _ = (pair, triple);
    }
}
```

### Encadenamiento de combinadores

```rust
fn valid_username() -> impl Strategy<Value = String> {
    "[a-zA-Z][a-zA-Z0-9_]{2,19}"          // 3 a 20 chars, empieza con letra
        .prop_filter("no palabras reservadas", |s| {
            !["admin", "root", "system"].contains(&s.as_str())
        })
        .prop_map(|s| s.to_lowercase())     // normalizar a minúsculas
}

fn valid_password() -> impl Strategy<Value = String> {
    // Al menos 1 mayúscula, 1 minúscula, 1 dígito, 8-20 chars
    (
        "[A-Z]",
        "[a-z]{3,10}",
        "[0-9]{2,4}",
        "[!@#$%]{1,3}",
    )
        .prop_map(|(upper, lower, digits, special)| {
            format!("{}{}{}{}", upper, lower, digits, special)
        })
        .prop_shuffle()  // mezclar los caracteres
}
```

### prop_shuffle: permutar colecciones

```rust
proptest! {
    #[test]
    fn test_shuffle(
        v in proptest::collection::vec(1..100i32, 10)
            .prop_shuffle()
    ) {
        // v tiene los mismos elementos pero en orden aleatorio
        prop_assert_eq!(v.len(), 10);
    }
}
```

---

## 12. prop_oneof! y Union de estrategias

`prop_oneof!` selecciona una estrategia de entre varias, útil para generar datos polimórficos o con distribuciones específicas.

### Uso básico

```rust
proptest! {
    #[test]
    fn test_oneof(
        // Selecciona entre las tres estrategias con igual probabilidad
        n in prop_oneof![
            0..10i32,       // ~33%: número pequeño
            100..1000i32,   // ~33%: número mediano  
            -1000..-100i32, // ~33%: número negativo grande
        ],
    ) {
        prop_assert!(
            (0..10).contains(&n) || 
            (100..1000).contains(&n) || 
            (-1000..-100).contains(&n)
        );
    }
}
```

### Con pesos

```rust
use proptest::strategy::Union;

fn weighted_number() -> impl Strategy<Value = i32> {
    // El número es la probabilidad relativa
    Union::new_weighted(vec![
        (8, (0..10i32).boxed()),       // 80%: número pequeño
        (1, (10..100i32).boxed()),     // 10%: número mediano
        (1, (100..1000i32).boxed()),   // 10%: número grande
    ])
}

proptest! {
    #[test]
    fn test_weighted(n in weighted_number()) {
        prop_assert!(n >= 0 && n < 1000);
    }
}
```

### Generar variantes de enum

```rust
#[derive(Debug, Clone, PartialEq)]
enum HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Patch,
}

fn http_method_strategy() -> impl Strategy<Value = HttpMethod> {
    prop_oneof![
        Just(HttpMethod::Get),
        Just(HttpMethod::Post),
        Just(HttpMethod::Put),
        Just(HttpMethod::Delete),
        Just(HttpMethod::Patch),
    ]
}

#[derive(Debug, Clone)]
enum JsonValue {
    Null,
    Bool(bool),
    Number(f64),
    Str(String),
    Array(Vec<JsonValue>),
    Object(Vec<(String, JsonValue)>),
}

fn json_value_strategy() -> impl Strategy<Value = JsonValue> {
    let leaf = prop_oneof![
        Just(JsonValue::Null),
        any::<bool>().prop_map(JsonValue::Bool),
        proptest::num::f64::NORMAL.prop_map(JsonValue::Number),
        "[a-zA-Z0-9 ]{0,20}".prop_map(JsonValue::Str),
    ];
    
    leaf.prop_recursive(
        3,    // profundidad máxima
        50,   // nodos máximos
        5,    // items por nivel de recursión
        |inner| {
            prop_oneof![
                // Array de 0 a 5 elementos
                proptest::collection::vec(inner.clone(), 0..5)
                    .prop_map(JsonValue::Array),
                // Object de 0 a 5 pares
                proptest::collection::vec(
                    ("[a-z]{1,8}", inner),
                    0..5,
                ).prop_map(JsonValue::Object),
            ]
        },
    )
}
```

---

## 13. Generar enums con estrategias

Los enums de Rust se generan componiendo `prop_oneof!` con estrategias para los datos de cada variante.

### Enum simple sin datos

```rust
#[derive(Debug, Clone, Copy, PartialEq)]
enum Direction {
    North,
    South,
    East,
    West,
}

fn direction_strategy() -> impl Strategy<Value = Direction> {
    prop_oneof![
        Just(Direction::North),
        Just(Direction::South),
        Just(Direction::East),
        Just(Direction::West),
    ]
}
```

### Enum con datos

```rust
#[derive(Debug, Clone, PartialEq)]
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
    Triangle(f64, f64, f64),
}

fn shape_strategy() -> impl Strategy<Value = Shape> {
    prop_oneof![
        (0.1f64..100.0).prop_map(Shape::Circle),
        (0.1f64..100.0, 0.1f64..100.0)
            .prop_map(|(w, h)| Shape::Rectangle(w, h)),
        // Para triángulos, asegurar desigualdad triangular
        (0.1f64..50.0, 0.1f64..50.0)
            .prop_flat_map(|(a, b)| {
                let min_c = (a - b).abs() + 0.01;
                let max_c = a + b - 0.01;
                (Just(a), Just(b), min_c..max_c)
            })
            .prop_map(|(a, b, c)| Shape::Triangle(a, b, c)),
    ]
}

impl Shape {
    fn area(&self) -> f64 {
        match self {
            Shape::Circle(r) => std::f64::consts::PI * r * r,
            Shape::Rectangle(w, h) => w * h,
            Shape::Triangle(a, b, c) => {
                let s = (a + b + c) / 2.0;
                (s * (s - a) * (s - b) * (s - c)).sqrt()
            }
        }
    }
}

proptest! {
    #[test]
    fn test_shape_area_positive(shape in shape_strategy()) {
        let area = shape.area();
        prop_assert!(area > 0.0, "area={} for {:?}", area, shape);
    }
}
```

### Enum con struct variants

```rust
#[derive(Debug, Clone)]
enum Command {
    CreateUser { name: String, email: String },
    DeleteUser { id: u64 },
    UpdateEmail { id: u64, new_email: String },
    ListUsers { page: u32, per_page: u32 },
}

fn command_strategy() -> impl Strategy<Value = Command> {
    prop_oneof![
        ("[a-z]{3,15}", "[a-z]{3,10}@[a-z]{3,8}\\.com")
            .prop_map(|(name, email)| Command::CreateUser { name, email }),
        any::<u64>()
            .prop_map(|id| Command::DeleteUser { id }),
        (any::<u64>(), "[a-z]{3,10}@[a-z]{3,8}\\.com")
            .prop_map(|(id, new_email)| Command::UpdateEmail { id, new_email }),
        (0..100u32, prop_oneof![Just(10u32), Just(25), Just(50), Just(100)])
            .prop_map(|(page, per_page)| Command::ListUsers { page, per_page }),
    ]
}
```

---

## 14. Implementar Arbitrary para tipos propios

El trait `Arbitrary` permite usar `any::<MiTipo>()` directamente. Es útil cuando un tipo se usa frecuentemente en tests y tiene una forma "natural" de generarse.

### Implementación básica

```rust
use proptest::prelude::*;
use proptest::arbitrary::{Arbitrary, StrategyFor};

#[derive(Debug, Clone, PartialEq)]
struct Email {
    local: String,
    domain: String,
}

impl Email {
    fn full(&self) -> String {
        format!("{}@{}", self.local, self.domain)
    }
}

impl Arbitrary for Email {
    type Parameters = ();
    type Strategy = BoxedStrategy<Self>;

    fn arbitrary_with(_args: Self::Parameters) -> Self::Strategy {
        (
            "[a-z][a-z0-9.]{1,14}",   // local part
            "[a-z]{3,10}\\.(com|org|net)", // domain
        )
            .prop_map(|(local, domain)| Email { local, domain })
            .boxed()
    }
}

// Ahora funciona:
proptest! {
    #[test]
    fn test_email_format(email in any::<Email>()) {
        let full = email.full();
        prop_assert!(full.contains('@'));
        prop_assert!(
            full.ends_with(".com") || 
            full.ends_with(".org") || 
            full.ends_with(".net")
        );
    }
}
```

### Arbitrary con parámetros

```rust
#[derive(Debug, Clone)]
struct BoundedVec {
    data: Vec<i32>,
}

impl Arbitrary for BoundedVec {
    // Los parámetros controlan la generación
    type Parameters = (usize, usize);  // (min_len, max_len)
    type Strategy = BoxedStrategy<Self>;

    fn arbitrary_with((min_len, max_len): Self::Parameters) -> Self::Strategy {
        proptest::collection::vec(any::<i32>(), min_len..=max_len)
            .prop_map(|data| BoundedVec { data })
            .boxed()
    }
}

proptest! {
    #[test]
    fn test_bounded_default(v in any::<BoundedVec>()) {
        // Con parámetros default (0, 0): vec vacío
        // Generalmente se define con valores útiles por defecto
        let _ = v;
    }
    
    #[test]
    fn test_bounded_custom(
        v in any_with::<BoundedVec>((5, 20))
    ) {
        prop_assert!(v.data.len() >= 5);
        prop_assert!(v.data.len() <= 20);
    }
}
```

### Cuándo implementar Arbitrary vs usar prop_compose!

```
┌────────────────────────┬──────────────────────────────────────┐
│ Usar prop_compose!     │ Usar Arbitrary                       │
├────────────────────────┼──────────────────────────────────────┤
│ Tipo usado en pocos    │ Tipo usado en muchos tests           │
│ tests                  │                                      │
│                        │                                      │
│ Diferentes tests       │ Existe UNA forma natural de generar  │
│ necesitan diferentes   │ el tipo                              │
│ configuraciones        │                                      │
│                        │                                      │
│ Estrategia simple,     │ Quieres usar any::<T>() por          │
│ una función basta      │ conveniencia                         │
│                        │                                      │
│ No quieres modificar   │ El tipo está en tu crate y puedes    │
│ el tipo (crate externo)│ añadir el impl                       │
└────────────────────────┴──────────────────────────────────────┘
```

---

## 15. ProptestConfig: configuración del runner

`ProptestConfig` controla cómo se ejecutan los property tests: cuántas iteraciones, semilla de aleatoriedad, manejo de regresiones, etc.

### Campos de configuración

```rust
use proptest::prelude::*;
use proptest::test_runner::Config as ProptestConfig;

let config = ProptestConfig {
    // Número de casos a generar (default: 256)
    cases: 256,
    
    // Máximo de veces que una estrategia puede rechazar un valor
    // antes de abortar (default: 65536)
    max_shrink_iters: 65536,
    
    // Máximo de valores rechazados por prop_filter/prop_assume
    // antes de abortar (default: 2^31)
    max_flat_map_regens: 1_000_000,
    
    // Fuente de la semilla de aleatoriedad
    // Default: buscar en archivo de regresión, luego RNG del sistema
    // rng_algorithm y fork se configuran por separado
    
    // Path del archivo de regresión
    // Default: mismo directorio que el test, con extensión .proptest-regressions
    source_file: Some("mi_test.rs"),
    
    // Otros campos usan defaults seguros
    ..ProptestConfig::default()
};
```

### Configuración inline con la macro

```rust
proptest! {
    // Configuración para todos los tests de este bloque
    #![proptest_config(ProptestConfig::with_cases(1000))]
    
    #[test]
    fn test_intensivo(x in any::<i32>()) {
        prop_assert_eq!(x.wrapping_add(0), x);
    }
}
```

### Configuración por test (forma manual)

```rust
#[test]
fn test_con_config_custom() {
    let config = ProptestConfig {
        cases: 5000,
        max_shrink_iters: 10_000,
        ..ProptestConfig::default()
    };
    
    let mut runner = proptest::test_runner::TestRunner::new(config);
    
    runner.run(&any::<Vec<u8>>(), |v| {
        // Se ejecuta 5000 veces
        prop_assert!(v.len() <= 100);
        Ok(())
    }).unwrap();
}
```

### Configuraciones recomendadas por escenario

```
┌───────────────────┬──────────┬──────────────────────────────────┐
│ Escenario         │ cases    │ Razón                            │
├───────────────────┼──────────┼──────────────────────────────────┤
│ Desarrollo rápido │ 100      │ Feedback loop corto              │
│ Default           │ 256      │ Balance velocidad/cobertura      │
│ CI normal         │ 1,000    │ Más cobertura, tiempo razonable  │
│ CI exhaustivo     │ 10,000   │ Antes de release                 │
│ Algorítmo crítico │ 100,000  │ Criptografía, finanzas           │
│ Fuzzing-like      │ 1,000,000│ Correr por la noche              │
└───────────────────┴──────────┴──────────────────────────────────┘
```

### Controlar cases con variable de entorno

```rust
proptest! {
    #![proptest_config(ProptestConfig::with_cases(
        std::env::var("PROPTEST_CASES")
            .ok()
            .and_then(|s| s.parse().ok())
            .unwrap_or(256)
    ))]
    
    #[test]
    fn test_configurable(x in any::<i32>()) {
        prop_assert_eq!(x.wrapping_mul(1), x);
    }
}
```

```bash
# Ejecución normal: 256 casos
cargo test

# CI: más casos
PROPTEST_CASES=5000 cargo test

# Pre-release: muchos casos
PROPTEST_CASES=100000 cargo test -- --test-threads=1
```

### Semilla fija para reproducibilidad

```rust
use proptest::test_runner::{TestRunner, Config};

#[test]
fn test_reproducible() {
    // Semilla fija: el test es 100% determinístico
    let config = Config {
        cases: 100,
        // La semilla se configura a través del TestRunner
        ..Config::default()
    };
    
    let mut runner = TestRunner::deterministic();
    
    runner.run(&(0..1000i32), |x| {
        prop_assert!(x < 1000)?;
        Ok(())
    }).unwrap();
}
```

---

## 16. Archivos de regresión (.proptest-regressions)

Cuando un property test falla, proptest guarda el input que causó el fallo en un archivo de regresión. La próxima vez que corras el test, ese input se re-testea **primero**, asegurando que el bug no reaparezca.

### Flujo de regresión

```
                    Primer fallo
    ┌───────────────────────────────────────┐
    │ proptest genera input aleatorio       │
    │ → encuentra fallo con input X         │
    │ → shrink a caso mínimo X'            │
    │ → GUARDA X' en archivo de regresión   │
    │ → reporta X' como fallo              │
    └───────────────────┬───────────────────┘
                        │
                        ▼
                    Fix del bug
    ┌───────────────────────────────────────┐
    │ Desarrollador arregla el código       │
    │ → Re-corre tests                      │
    └───────────────────┬───────────────────┘
                        │
                        ▼
                 Ejecución post-fix
    ┌───────────────────────────────────────┐
    │ proptest LEE archivo de regresión     │
    │ → re-testea X' PRIMERO               │
    │ → si X' pasa: continúa con aleatorios │
    │ → si X' falla: reporta inmediatamente │
    └───────────────────────────────────────┘
```

### Ubicación y formato del archivo

```
mi_proyecto/
├── src/
│   ├── lib.rs
│   └── lib.rs.proptest-regressions    ← archivo de regresión
└── tests/
    ├── integration.rs
    └── integration.rs.proptest-regressions
```

El archivo de regresión tiene el mismo nombre que el archivo fuente con extensión `.proptest-regressions`:

```
# Seeds for failure cases proptest has found previously. It is
# temporary and can be deleted, but it speeds up re-testing.
cc 9fabe39e23bd3e78f431a7d8b6f9e3a4d9e2c1b0a3f6e8d7c5b4a3928170
```

### ¿Commitear o no commitear?

```
┌─────────────────────────────────────────────────────────────────┐
│              ¿.proptest-regressions en git?                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Opción A: Commitear (RECOMENDADO)                              │
│  ├── ✅ Todos corren los mismos tests de regresión              │
│  ├── ✅ Bugs encontrados se testean en CI                       │
│  ├── ✅ Protege contra regresiones en todo el equipo            │
│  └── ⚠️  Archivos crecen con el tiempo (limpiar periódicamente) │
│                                                                 │
│  Opción B: En .gitignore                                        │
│  ├── ✅ Repo más limpio                                         │
│  └── ❌ Cada developer tiene su propio historial de regresiones │
│                                                                 │
│  Decisión: commitear, especialmente en proyectos de equipo.     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Limpiar regresiones obsoletas

```bash
# Ver todos los archivos de regresión
find . -name "*.proptest-regressions"

# Borrar todos (los tests aleatorios los redescubrirán si el bug persiste)
find . -name "*.proptest-regressions" -delete

# Mejor: borrar selectivamente después de verificar que pasan
```

---

## 17. Shrinking en proptest: cómo funciona internamente

proptest usa **shrinking determinístico basado en valores**, a diferencia de quickcheck que usa shrinking basado en la historia de generación.

### Shrinking basado en valores vs basado en historia

```
  quickcheck (basado en historia):                
  ┌────────────────────────────────┐              
  │ 1. Genera valor V con RNG      │              
  │ 2. V falla el test             │              
  │ 3. Re-genera con misma semilla │              
  │    pero parámetros reducidos   │              
  │ 4. El nuevo valor puede ser    │              
  │    MUY diferente a V           │              
  │ 5. No determinístico           │              
  └────────────────────────────────┘              
                                                  
  proptest (basado en valor):                     
  ┌────────────────────────────────┐              
  │ 1. Genera valor V              │              
  │ 2. V falla el test             │              
  │ 3. Reduce V directamente:     │              
  │    V → V₁ → V₂ → ... → V_min  │              
  │ 4. Cada paso hace V más simple │              
  │ 5. 100% determinístico         │              
  └────────────────────────────────┘              
```

### Cómo se reduce cada tipo

```
┌──────────────┬──────────────────────────────────────────────┐
│ Tipo         │ Estrategia de shrinking                      │
├──────────────┼──────────────────────────────────────────────┤
│ i32          │ Reducir hacia 0: 1000 → 500 → 250 → ... → 1 │
│ u8           │ Reducir hacia 0: 200 → 100 → 50 → ... → 0   │
│ bool         │ true → false                                 │
│ char         │ Reducir codepoint, preferir ASCII             │
│ String       │ 1. Reducir longitud (eliminar chars)          │
│              │ 2. Simplificar chars restantes                │
│ Vec<T>       │ 1. Reducir longitud (eliminar elementos)      │
│              │ 2. Shrink cada elemento restante              │
│ (A, B)       │ Shrink A, luego shrink B                      │
│ Option<T>    │ Some(x) → None, o Some(shrink(x))            │
│ Result<T, E> │ Shrink el valor interior                      │
│ Enum         │ Shrink datos de la variante actual            │
└──────────────┴──────────────────────────────────────────────┘
```

### Ejemplo de shrinking paso a paso

```
  Input que falla: vec![97, 42, -31, 15, 88, -7, 3, 0, 55]
  Propiedad: "la suma de la lista debe ser positiva"
  
  Paso 1 — Reducir longitud del Vec:
    [97, 42, -31, 15, 88, -7, 3, 0, 55]  → falla (sum = 262)
    [97, 42, -31, 15, 88]                 → pasa  (sum = 211)
    [97, 42, -31, 15, 88, -7, 3]          → pasa  (sum = 207)
    [-31, 15, 88, -7, 3, 0, 55]           → pasa  (sum = 123)
    ...intentando eliminar diferentes subconjuntos...
    [-31]                                  → falla (sum = -31) ✓
    
  Paso 2 — Shrink el elemento:
    [-31] → falla
    [-16] → falla
    [-8]  → falla
    [-4]  → falla
    [-2]  → falla
    [-1]  → falla ← MÍNIMO ENCONTRADO
    [0]   → pasa

  Resultado: el caso mínimo es vec![-1]
  (Un solo número negativo viola "suma positiva")
```

### Control del shrinking

```rust
proptest! {
    #![proptest_config(ProptestConfig {
        // Máximo de iteraciones de shrinking (default: 2^32)
        max_shrink_iters: 1000,
        ..ProptestConfig::default()
    })]
    
    #[test]
    fn test_con_shrink_limitado(v in any::<Vec<i32>>()) {
        // Si el shrinking tarda mucho, se detendrá después de 1000 pasos
        prop_assert!(v.len() <= 100);
    }
}
```

---

## 18. prop_assert!, prop_assert_eq!, prop_assume!

Dentro de un bloque `proptest!`, se usan macros especiales en lugar de las macros estándar de Rust.

### ¿Por qué macros especiales?

```
  assert! estándar:
  ┌──────────────────────────────────────┐
  │ assert!(condición);                  │
  │ Si falla → panic!                    │
  │ El panic interrumpe el shrinking     │
  │ NO muestra el input reducido         │
  └──────────────────────────────────────┘

  prop_assert!:
  ┌──────────────────────────────────────┐
  │ prop_assert!(condición);             │
  │ Si falla → return Err(TestCaseError) │
  │ proptest captura el error            │
  │ Inicia shrinking                     │
  │ Muestra el input MÍNIMO que falla   │
  └──────────────────────────────────────┘
```

### prop_assert! y variantes

```rust
proptest! {
    #[test]
    fn demo_asserts(x in 0..100i32, y in 0..100i32) {
        // prop_assert! — verificar una condición booleana
        prop_assert!(x >= 0);
        prop_assert!(x < 100, "x debe ser < 100, pero fue {}", x);
        
        // prop_assert_eq! — verificar igualdad (muestra diff si falla)
        let sum = x + y;
        prop_assert_eq!(sum, x + y);
        
        // prop_assert_ne! — verificar desigualdad
        if x != 0 {
            prop_assert_ne!(x, 0);
        }
    }
}
```

### Mensajes de error custom

```rust
proptest! {
    #[test]
    fn con_mensajes(v in proptest::collection::vec(any::<i32>(), 1..20)) {
        let sorted = {
            let mut s = v.clone();
            s.sort();
            s
        };
        
        // Mensaje con formato
        prop_assert_eq!(
            sorted.len(), v.len(),
            "sort cambió la longitud: original={}, sorted={}",
            v.len(), sorted.len()
        );
        
        // Verificar que está ordenado
        for i in 1..sorted.len() {
            prop_assert!(
                sorted[i-1] <= sorted[i],
                "No ordenado en posición {}: {} > {}",
                i, sorted[i-1], sorted[i]
            );
        }
    }
}
```

### prop_assume! — filtrar inputs en runtime

`prop_assume!` descarta el caso actual si la condición no se cumple, sin contar como fallo:

```rust
proptest! {
    #[test]
    fn division_segura(a in any::<i32>(), b in any::<i32>()) {
        // Descartar divisiones por cero y overflow
        prop_assume!(b != 0);
        prop_assume!(!(a == i32::MIN && b == -1));
        
        let result = a / b;
        let remainder = a % b;
        
        // Propiedad: a == b * (a/b) + a%b
        prop_assert_eq!(a, b * result + remainder);
    }
}
```

### prop_assume! vs prop_filter

```
┌─────────────────────┬──────────────────────────────────────────┐
│ prop_assume!         │ prop_filter                              │
├─────────────────────┼──────────────────────────────────────────┤
│ Dentro del cuerpo    │ En la definición de la estrategia       │
│ del test             │                                          │
│                      │                                          │
│ Evalúa DESPUÉS de   │ Filtra ANTES de generar el valor         │
│ generar el valor     │ completo                                │
│                      │                                          │
│ Más simple           │ Más eficiente (no genera valores que    │
│                      │ serán descartados)                      │
│                      │                                          │
│ Bueno para pre-      │ Bueno para restricciones conocidas en   │
│ condiciones simples  │ la definición del dominio               │
│                      │                                          │
│ Funciona con inputs  │ Solo funciona con la estrategia a la    │
│ combinados           │ que se aplica                           │
└─────────────────────┴──────────────────────────────────────────┘
```

---

## 19. Estrategias dependientes: prop_flat_map

`prop_flat_map` genera una estrategia cuya forma depende de un valor generado previamente. Es el equivalente de `flat_map`/`and_then` de iteradores/futures.

### Diferencia entre prop_map y prop_flat_map

```
  prop_map:
    Estrategia<A> → (A → B) → Estrategia<B>
    Transforma el VALOR, la forma de la estrategia no cambia.

  prop_flat_map:
    Estrategia<A> → (A → Estrategia<B>) → Estrategia<B>
    Usa el valor A para CREAR una nueva estrategia de B.
    La forma de B depende del valor concreto de A.
```

### Ejemplo: Vec con su índice válido

```rust
proptest! {
    #[test]
    fn vec_con_indice(
        // Generar un Vec, luego un índice válido dentro de él
        (v, idx) in proptest::collection::vec(any::<i32>(), 1..20)
            .prop_flat_map(|v| {
                let len = v.len();
                (Just(v), 0..len)
            })
    ) {
        // idx siempre es un índice válido para v
        let _element = v[idx]; // nunca panic
        prop_assert!(idx < v.len());
    }
}
```

### Ejemplo: string y substring

```rust
proptest! {
    #[test]
    fn string_contiene_substring(
        (s, start, end) in "[a-z]{5,20}"
            .prop_flat_map(|s| {
                let len = s.len();
                (Just(s), (0..len).prop_flat_map(move |start| {
                    (Just(start), start..=len)
                }))
            })
            .prop_map(|(s, (start, end))| (s, start, end))
    ) {
        let substring = &s[start..end];
        prop_assert!(s.contains(substring));
    }
}
```

### Ejemplo: matriz con dimensiones coherentes

```rust
fn coherent_matrix() -> impl Strategy<Value = (usize, usize, Vec<Vec<f64>>)> {
    (1..10usize, 1..10usize)
        .prop_flat_map(|(rows, cols)| {
            let row_strategy = proptest::collection::vec(
                proptest::num::f64::NORMAL,
                cols,  // ← depende del valor generado
            );
            (
                Just(rows),
                Just(cols),
                proptest::collection::vec(row_strategy, rows), // ← depende también
            )
        })
}

proptest! {
    #[test]
    fn test_matrix(
        (rows, cols, matrix) in coherent_matrix()
    ) {
        prop_assert_eq!(matrix.len(), rows);
        for row in &matrix {
            prop_assert_eq!(row.len(), cols);
        }
    }
}
```

### prop_flat_map vs prop_compose! de dos etapas

Ambos logran lo mismo; `prop_compose!` con dos etapas es azúcar sintáctica para `prop_flat_map`:

```rust
// Usando prop_flat_map directamente
fn sized_vec_flat() -> impl Strategy<Value = (usize, Vec<i32>)> {
    (1..20usize).prop_flat_map(|size| {
        (
            Just(size),
            proptest::collection::vec(any::<i32>(), size),
        )
    })
}

// Equivalente con prop_compose!
prop_compose! {
    fn sized_vec_compose()(
        size in 1..20usize
    )(
        size in Just(size),
        data in proptest::collection::vec(any::<i32>(), size)
    ) -> (usize, Vec<i32>) {
        (size, data)
    }
}
```

---

## 20. Testing de máquinas de estado con proptest

Una aplicación avanzada de property testing es verificar máquinas de estado generando secuencias aleatorias de operaciones.

### Concepto

```
  En lugar de testear operaciones individuales:
    insert(5) → OK
    insert(3) → OK
    remove(5) → OK
    
  Generamos SECUENCIAS ALEATORIAS de operaciones:
    [Insert(42), Insert(-7), Remove(42), Insert(100), Get(42), ...]
    
  Y verificamos INVARIANTES después de cada operación:
    - La estructura es consistente
    - Las operaciones producen resultados correctos
    - El estado final es válido
```

### Ejemplo: testing de un HashMap casero

```rust
use std::collections::HashMap;

// Nuestro "sistema bajo test" (simplificado)
struct SimpleMap {
    data: Vec<(String, i32)>,
}

impl SimpleMap {
    fn new() -> Self {
        SimpleMap { data: Vec::new() }
    }
    
    fn insert(&mut self, key: String, value: i32) {
        // Eliminar duplicados
        self.data.retain(|(k, _)| k != &key);
        self.data.push((key, value));
    }
    
    fn get(&self, key: &str) -> Option<i32> {
        self.data.iter()
            .rev()
            .find(|(k, _)| k == key)
            .map(|(_, v)| *v)
    }
    
    fn remove(&mut self, key: &str) -> Option<i32> {
        if let Some(pos) = self.data.iter().position(|(k, _)| k == key) {
            Some(self.data.remove(pos).1)
        } else {
            None
        }
    }
    
    fn len(&self) -> usize {
        self.data.len()
    }
}

// Definir operaciones como enum
#[derive(Debug, Clone)]
enum MapOp {
    Insert(String, i32),
    Get(String),
    Remove(String),
}

// Estrategia para generar operaciones
fn map_op_strategy() -> impl Strategy<Value = MapOp> {
    let key = "[a-z]{1,5}";
    prop_oneof![
        (key, any::<i32>()).prop_map(|(k, v)| MapOp::Insert(k, v)),
        key.prop_map(MapOp::Get),
        key.prop_map(|k| MapOp::Remove(k.to_string())),
    ]
}

proptest! {
    #[test]
    fn test_map_vs_oracle(
        ops in proptest::collection::vec(map_op_strategy(), 1..50)
    ) {
        // Modelo oracle: HashMap estándar
        let mut oracle = HashMap::new();
        let mut sut = SimpleMap::new(); // System Under Test
        
        for op in ops {
            match op {
                MapOp::Insert(k, v) => {
                    oracle.insert(k.clone(), v);
                    sut.insert(k, v);
                }
                MapOp::Get(k) => {
                    let expected = oracle.get(&k).copied();
                    let actual = sut.get(&k);
                    prop_assert_eq!(
                        actual, expected,
                        "get('{}') diverge: sut={:?}, oracle={:?}",
                        k, actual, expected
                    );
                }
                MapOp::Remove(k) => {
                    let expected = oracle.remove(&k);
                    let actual = sut.remove(&k);
                    prop_assert_eq!(
                        actual, expected,
                        "remove('{}') diverge", k
                    );
                }
            }
        }
        
        // Invariante final: misma cantidad de elementos
        prop_assert_eq!(sut.len(), oracle.len());
    }
}
```

### Diagrama del testing de máquina de estado

```
  ┌────────────────────────────────────────────────────────────┐
  │                 PROPERTY TEST DE ESTADO                    │
  │                                                            │
  │   proptest genera:                                         │
  │   ops = [Insert("a",1), Insert("b",2), Get("a"),          │
  │          Remove("b"), Get("b"), Insert("a",3)]             │
  │                                                            │
  │   Ejecución paralela:                                      │
  │                                                            │
  │   Oracle (HashMap)          SUT (SimpleMap)                │
  │   ┌──────────────┐         ┌──────────────┐               │
  │   │ insert(a, 1) │         │ insert(a, 1) │               │
  │   │ insert(b, 2) │         │ insert(b, 2) │               │
  │   │ get(a) → 1   │ ══?══  │ get(a) → 1   │  ✓ iguales   │
  │   │ remove(b) → 2│ ══?══  │ remove(b) → 2│  ✓ iguales   │
  │   │ get(b) → None│ ══?══  │ get(b) → None│  ✓ iguales   │
  │   │ insert(a, 3) │         │ insert(a, 3) │               │
  │   │ len() → 1    │ ══?══  │ len() → 1    │  ✓ iguales   │
  │   └──────────────┘         └──────────────┘               │
  │                                                            │
  │   Si alguna comparación falla → shrinking reduce la        │
  │   secuencia de ops al mínimo que reproduce el fallo.       │
  └────────────────────────────────────────────────────────────┘
```

---

## 21. Patrones avanzados y trucos

### Reutilizar estrategias con funciones

```rust
// Definir estrategias como funciones retorna impl Strategy
fn positive_i32() -> impl Strategy<Value = i32> {
    1..=i32::MAX
}

fn ascii_string(max_len: usize) -> impl Strategy<Value = String> {
    proptest::collection::vec(0x20u8..0x7F, 0..max_len)
        .prop_map(|bytes| String::from_utf8(bytes).unwrap())
}

fn nonempty_vec<S: Strategy>(element: S, max_len: usize) -> impl Strategy<Value = Vec<S::Value>> {
    proptest::collection::vec(element, 1..max_len)
}
```

### Condicionar tests con prop_assume!

```rust
proptest! {
    #[test]
    fn test_division_euclidiana(a in any::<i64>(), b in any::<i64>()) {
        prop_assume!(b != 0);
        prop_assume!(!(a == i64::MIN && b == -1)); // evitar overflow
        
        let q = a / b;
        let r = a % b;
        
        // Propiedad fundamental de la división euclidiana
        prop_assert_eq!(a, b * q + r);
        
        // El resto tiene el mismo signo que el dividendo (en Rust)
        if r != 0 {
            prop_assert_eq!(a.signum(), r.signum());
        }
    }
}
```

### Debug output con proptest

```rust
proptest! {
    #[test]
    fn test_con_debug(v in proptest::collection::vec(0..100i32, 1..10)) {
        let original = v.clone();
        let mut sorted = v;
        sorted.sort();
        
        // Si falla, prop_assert! muestra `v` pero no `sorted`
        // Para ver valores intermedios, incluirlos en el mensaje
        prop_assert_eq!(
            sorted.len(), original.len(),
            "\noriginal: {:?}\nsorted:   {:?}",
            original, sorted
        );
    }
}
```

### Estrategia condicional: generar datos válidos directamente

```rust
// En vez de generar y filtrar, construir datos válidos por diseño

// ❌ Ineficiente: generar y filtrar triángulos válidos
fn triangle_bad() -> impl Strategy<Value = (f64, f64, f64)> {
    (0.1f64..100.0, 0.1f64..100.0, 0.1f64..100.0)
        .prop_filter("desigualdad triangular", |&(a, b, c)| {
            a + b > c && a + c > b && b + c > a
        })
}

// ✅ Eficiente: generar triángulos válidos por construcción
fn triangle_good() -> impl Strategy<Value = (f64, f64, f64)> {
    // Generar tres valores positivos y construir lados válidos
    (0.1f64..50.0, 0.1f64..50.0, 0.1f64..50.0)
        .prop_map(|(x, y, z)| {
            // Con esta transformación, a+b > c siempre se cumple
            let a = x + y;
            let b = y + z;
            let c = x + z;
            (a, b, c)
        })
}
```

### Boxed strategies para tipos dinámicos

```rust
use proptest::strategy::BoxedStrategy;

// Cuando necesitas retornar diferentes estrategias según una condición,
// o almacenar estrategias en colecciones

fn strategy_for_size(size: &str) -> BoxedStrategy<Vec<i32>> {
    match size {
        "small" => proptest::collection::vec(any::<i32>(), 0..5).boxed(),
        "medium" => proptest::collection::vec(any::<i32>(), 5..50).boxed(),
        "large" => proptest::collection::vec(any::<i32>(), 50..500).boxed(),
        _ => proptest::collection::vec(any::<i32>(), 0..100).boxed(),
    }
}
```

### No-shrink: desactivar shrinking para un valor

```rust
use proptest::strategy::Strategy;

proptest! {
    #[test]
    fn test_sin_shrink(
        // .no_shrink() evita que proptest intente reducir este valor
        x in (0..1000i32).no_shrink()
    ) {
        // Si falla, reporta el valor original sin intentar reducirlo
        // Útil cuando el shrinking es costoso o confuso
        prop_assert!(x < 1000);
    }
}
```

---

## 22. Comparación con C y Go

### C: no tiene framework estándar de PBT

```c
// C: usar theft (el más popular) o implementar manualmente
// theft requiere definir: alloc, fill (generador), check (propiedad)

#include <theft.h>

// Definir generador
static enum theft_alloc_res
int_alloc(struct theft *t, void *env, void **output) {
    int *n = malloc(sizeof(int));
    if (!n) return THEFT_ALLOC_ERROR;
    *n = (int)theft_random_bits(t, 32);
    *output = n;
    return THEFT_ALLOC_OK;
}

// Definir propiedad
static enum theft_trial_res
prop_add_commutative(struct theft *t, void *arg1, void *arg2) {
    int a = *(int *)arg1;
    int b = *(int *)arg2;
    return (a + b == b + a) ? THEFT_TRIAL_PASS : THEFT_TRIAL_FAIL;
}

// Ejecutar
void test_add() {
    struct theft *t = theft_init(0);
    struct theft_run_config cfg = {
        .prop2 = prop_add_commutative,
        .type_info = { &int_type_info, &int_type_info },
        .trials = 1000,
    };
    theft_run(t, &cfg);
    theft_free(t);
}

// C no tiene:
//   - Generadores automáticos para tipos
//   - Composición funcional
//   - Regex strategies
//   - Macros ergonómicas
//   - Shrinking integrado con tipos
```

### Go: testing/quick (built-in, limitado) o gopter

```go
// Go: testing/quick (built-in pero limitado)
package mypackage

import (
    "testing"
    "testing/quick"
)

func TestAddCommutative(t *testing.T) {
    f := func(a, b int) bool {
        return a+b == b+a
    }
    // quick.Check genera inputs aleatorios y verifica f
    if err := quick.Check(f, nil); err != nil {
        t.Error(err)
    }
}

// Limitaciones de testing/quick:
//   - Solo tipos que implementan Generate (básicos)
//   - Shrinking muy básico
//   - Sin composición de generadores
//   - Sin regex strategies

// gopter: alternativa más potente (similar a proptest)
import "github.com/leanovate/gopter"
import "github.com/leanovate/gopter/gen"

func TestWithGopter(t *testing.T) {
    parameters := gopter.DefaultTestParameters()
    parameters.MinSuccessfulTests = 1000
    
    properties := gopter.NewProperties(parameters)
    
    properties.Property("add is commutative", prop.ForAll(
        func(a, b int) bool {
            return a+b == b+a
        },
        gen.Int(),
        gen.Int(),
    ))
    
    properties.TestingRun(t)
}
```

### Tabla comparativa

```
┌───────────────────┬────────────────┬────────────────┬────────────────┐
│ Característica    │ C (theft)      │ Go (quick/     │ Rust (proptest)│
│                   │                │ gopter)        │                │
├───────────────────┼────────────────┼────────────────┼────────────────┤
│ Built-in          │ No             │ testing/quick  │ No (crate)     │
│                   │                │ (básico)       │                │
│ Setup             │ Manual,        │ Mínimo         │ 1 línea en     │
│                   │ verboso        │                │ Cargo.toml     │
│ Generadores       │ Manuales       │ Básicos/       │ Ricos, auto-   │
│                   │ (alloc+fill)   │ manuales       │ máticos        │
│ Composición       │ No             │ gopter sí      │ Funcional      │
│                   │                │                │ completa       │
│ Regex strategies  │ No             │ No             │ Sí             │
│ Shrinking         │ Básico         │ quick: no      │ Determinístico │
│                   │                │ gopter: sí     │                │
│ Regresión auto    │ No             │ No             │ Sí (archivos)  │
│ Type safety       │ void*          │ interface{}    │ Genéricos       │
│                   │ (no seguro)    │ o genéricos    │ (seguro)       │
│ Ergonomía         │ Baja           │ Media          │ Alta (macros)  │
│ Líneas para       │ ~30            │ ~10 (quick)    │ ~5             │
│ test simple       │                │ ~15 (gopter)   │                │
└───────────────────┴────────────────┴────────────────┴────────────────┘
```

---

## 23. Errores comunes con proptest

### Error 1: usar assert! en lugar de prop_assert!

```rust
// ❌ INCORRECTO: assert! causa panic, no integra con shrinking
proptest! {
    #[test]
    fn test_malo(x in any::<i32>()) {
        assert!(x > 0); // Si falla: panic sin shrinking útil
    }
}

// ✅ CORRECTO: prop_assert! retorna error, permite shrinking
proptest! {
    #[test]
    fn test_bueno(x in any::<i32>()) {
        prop_assert!(x != i32::MIN || true); // ejemplo
    }
}
```

### Error 2: filtros demasiado agresivos

```rust
// ❌ INCORRECTO: rechaza >99% de los valores
proptest! {
    #[test]
    fn test_rechazador(
        v in any::<Vec<i32>>()
            .prop_filter("ordenado", |v| {
                v.windows(2).all(|w| w[0] <= w[1])
            })
    ) {
        // Probablemente falla con "too many rejections"
    }
}

// ✅ CORRECTO: generar datos válidos por construcción
proptest! {
    #[test]
    fn test_construido(
        v in proptest::collection::vec(any::<i32>(), 0..20)
            .prop_map(|mut v| { v.sort(); v })
    ) {
        // v siempre está ordenado
        for w in v.windows(2) {
            prop_assert!(w[0] <= w[1]);
        }
    }
}
```

### Error 3: no manejar overflow de enteros

```rust
// ❌ INCORRECTO: overflow con any::<i32>()
proptest! {
    #[test]
    fn test_overflow(a in any::<i32>(), b in any::<i32>()) {
        let sum = a + b;  // ¡PANIC en debug! (overflow)
        prop_assert_eq!(sum - b, a);
    }
}

// ✅ CORRECTO: usar wrapping o rangos acotados
proptest! {
    #[test]
    fn test_sin_overflow(a in any::<i32>(), b in any::<i32>()) {
        let sum = a.wrapping_add(b);
        prop_assert_eq!(sum.wrapping_sub(b), a);
    }
}

// ✅ ALTERNATIVA: acotar rangos
proptest! {
    #[test]
    fn test_acotado(a in -1000..1000i32, b in -1000..1000i32) {
        let sum = a + b;  // No puede hacer overflow con estos rangos
        prop_assert_eq!(sum - b, a);
    }
}
```

### Error 4: ignorar que any::\<f64\>() genera NaN e Inf

```rust
// ❌ INCORRECTO: no considera NaN
proptest! {
    #[test]
    fn test_float_malo(x in any::<f64>()) {
        prop_assert_eq!(x, x);  // ¡FALLA! NaN != NaN
    }
}

// ✅ CORRECTO: usar subconjunto finito
proptest! {
    #[test]
    fn test_float_bueno(x in proptest::num::f64::NORMAL) {
        prop_assert_eq!(x, x);  // OK: NORMAL excluye NaN
    }
}
```

### Error 5: no commitear archivos de regresión

```bash
# ❌ INCORRECTO: .gitignore incluye proptest-regressions
*.proptest-regressions

# ✅ CORRECTO: commitear para compartir regresiones
# No incluir *.proptest-regressions en .gitignore
git add *.proptest-regressions
git commit -m "add proptest regression files"
```

### Error 6: tests de propiedad vacíos o triviales

```rust
// ❌ INÚTIL: no verifica nada real
proptest! {
    #[test]
    fn test_trivial(x in any::<i32>()) {
        prop_assert!(true);  // Siempre pasa
    }
}

// ❌ INÚTIL: verifica algo ya garantizado por el tipo
proptest! {
    #[test]
    fn test_ya_garantizado(x in 0..100i32) {
        prop_assert!(x >= 0);  // Obvio por la estrategia
        prop_assert!(x < 100); // Obvio por la estrategia
    }
}

// ✅ ÚTIL: verifica una propiedad del código bajo test
proptest! {
    #[test]
    fn test_real(v in proptest::collection::vec(any::<i32>(), 0..50)) {
        let mut sorted = v.clone();
        sorted.sort();
        
        // Propiedades NO triviales del sort:
        prop_assert_eq!(sorted.len(), v.len());        // preserva longitud
        for w in sorted.windows(2) {
            prop_assert!(w[0] <= w[1]);                // resultado ordenado
        }
        // Mismo multiconjunto (cada elemento aparece igual cantidad)
        let mut orig = v;
        orig.sort();
        prop_assert_eq!(sorted, orig);
    }
}
```

### Error 7: prop_compose! sin Just() en la segunda etapa

```rust
// ❌ INCORRECTO: re-genera `size` en la segunda etapa
prop_compose! {
    fn bad_strategy()(
        size in 1..20usize
    )(
        size in 1..20usize,  // ← ¡Se genera otro valor diferente!
        data in proptest::collection::vec(any::<i32>(), size)
    ) -> (usize, Vec<i32>) {
        (size, data) // size y data.len() pueden no coincidir
    }
}

// ✅ CORRECTO: usar Just() para pasar valores entre etapas
prop_compose! {
    fn good_strategy()(
        size in 1..20usize
    )(
        size in Just(size),  // ← Mismo valor que la primera etapa
        data in proptest::collection::vec(any::<i32>(), size)
    ) -> (usize, Vec<i32>) {
        (size, data) // size == data.len() siempre
    }
}
```

### Error 8: olvidar el ? en prop_assert! (forma manual)

```rust
// ❌ INCORRECTO: sin ? el error se ignora
#[test]
fn test_manual_malo() {
    let mut runner = proptest::test_runner::TestRunner::default();
    runner.run(&any::<i32>(), |x| {
        prop_assert!(x > 0); // ← sin ? no se propaga el error
        Ok(())               // siempre retorna Ok
    }).unwrap();
}

// ✅ CORRECTO: ? propaga el error
#[test]
fn test_manual_bueno() {
    let mut runner = proptest::test_runner::TestRunner::default();
    runner.run(&any::<i32>(), |x| {
        prop_assert!(x > 0)?; // ← ? propaga el TestCaseError
        Ok(())
    }).unwrap();
}
```

---

## 24. Ejemplo completo: validador de expresiones matemáticas

Este ejemplo implementa un parser y evaluador de expresiones matemáticas simples, y usa proptest para verificar propiedades del sistema completo.

### Estructura del proyecto

```
math_expr/
├── Cargo.toml
├── src/
│   └── lib.rs         # Parser + evaluador
└── tests/
    └── properties.rs  # Property tests
```

### Cargo.toml

```toml
[package]
name = "math_expr"
version = "0.1.0"
edition = "2021"

[dev-dependencies]
proptest = "1"
```

### src/lib.rs

```rust
/// Expresiones matemáticas simples
#[derive(Debug, Clone, PartialEq)]
pub enum Expr {
    /// Número literal
    Num(f64),
    /// Negación: -expr
    Neg(Box<Expr>),
    /// Suma: expr + expr
    Add(Box<Expr>, Box<Expr>),
    /// Resta: expr - expr
    Sub(Box<Expr>, Box<Expr>),
    /// Multiplicación: expr * expr
    Mul(Box<Expr>, Box<Expr>),
}

impl Expr {
    /// Evaluar la expresión
    pub fn eval(&self) -> f64 {
        match self {
            Expr::Num(n) => *n,
            Expr::Neg(e) => -e.eval(),
            Expr::Add(a, b) => a.eval() + b.eval(),
            Expr::Sub(a, b) => a.eval() - b.eval(),
            Expr::Mul(a, b) => a.eval() * b.eval(),
        }
    }

    /// Convertir a string con paréntesis explícitos
    pub fn to_string_explicit(&self) -> String {
        match self {
            Expr::Num(n) => {
                if *n < 0.0 {
                    format!("({})", n)
                } else {
                    format!("{}", n)
                }
            }
            Expr::Neg(e) => format!("(-{})", e.to_string_explicit()),
            Expr::Add(a, b) => {
                format!("({} + {})", a.to_string_explicit(), b.to_string_explicit())
            }
            Expr::Sub(a, b) => {
                format!("({} - {})", a.to_string_explicit(), b.to_string_explicit())
            }
            Expr::Mul(a, b) => {
                format!("({} * {})", a.to_string_explicit(), b.to_string_explicit())
            }
        }
    }

    /// Contar nodos en la expresión
    pub fn node_count(&self) -> usize {
        match self {
            Expr::Num(_) => 1,
            Expr::Neg(e) => 1 + e.node_count(),
            Expr::Add(a, b) | Expr::Sub(a, b) | Expr::Mul(a, b) => {
                1 + a.node_count() + b.node_count()
            }
        }
    }

    /// Simplificar la expresión (optimizaciones algebraicas)
    pub fn simplify(&self) -> Expr {
        match self {
            Expr::Num(n) => Expr::Num(*n),
            Expr::Neg(e) => {
                let inner = e.simplify();
                match inner {
                    // --x = x
                    Expr::Neg(inner_inner) => *inner_inner,
                    // -0 = 0
                    Expr::Num(n) if n == 0.0 => Expr::Num(0.0),
                    other => Expr::Neg(Box::new(other)),
                }
            }
            Expr::Add(a, b) => {
                let a = a.simplify();
                let b = b.simplify();
                match (&a, &b) {
                    // x + 0 = x
                    (_, Expr::Num(n)) if *n == 0.0 => a,
                    // 0 + x = x
                    (Expr::Num(n), _) if *n == 0.0 => b,
                    // constante + constante
                    (Expr::Num(x), Expr::Num(y)) => Expr::Num(x + y),
                    _ => Expr::Add(Box::new(a), Box::new(b)),
                }
            }
            Expr::Sub(a, b) => {
                let a = a.simplify();
                let b = b.simplify();
                match (&a, &b) {
                    // x - 0 = x
                    (_, Expr::Num(n)) if *n == 0.0 => a,
                    // constante - constante
                    (Expr::Num(x), Expr::Num(y)) => Expr::Num(x - y),
                    _ => Expr::Sub(Box::new(a), Box::new(b)),
                }
            }
            Expr::Mul(a, b) => {
                let a = a.simplify();
                let b = b.simplify();
                match (&a, &b) {
                    // x * 0 = 0
                    (_, Expr::Num(n)) if *n == 0.0 => Expr::Num(0.0),
                    // 0 * x = 0
                    (Expr::Num(n), _) if *n == 0.0 => Expr::Num(0.0),
                    // x * 1 = x
                    (_, Expr::Num(n)) if *n == 1.0 => a,
                    // 1 * x = x
                    (Expr::Num(n), _) if *n == 1.0 => b,
                    // constante * constante
                    (Expr::Num(x), Expr::Num(y)) => Expr::Num(x * y),
                    _ => Expr::Mul(Box::new(a), Box::new(b)),
                }
            }
        }
    }

    /// Parse de una expresión desde string (simplificado, solo con paréntesis explícitos)
    pub fn parse(input: &str) -> Result<Expr, String> {
        let input = input.trim();
        
        if input.is_empty() {
            return Err("empty input".to_string());
        }

        // Intentar como número
        if let Ok(n) = input.parse::<f64>() {
            return Ok(Expr::Num(n));
        }

        // Debe estar entre paréntesis
        if !input.starts_with('(') || !input.ends_with(')') {
            return Err(format!("expected parentheses: {}", input));
        }

        let inner = &input[1..input.len() - 1];

        // Negación: (-expr)
        if inner.starts_with('-') && !inner[1..].contains(['+', '-', '*']) {
            if let Ok(n) = inner.parse::<f64>() {
                return Ok(Expr::Num(n));
            }
            let sub_expr = Expr::parse(&inner[1..])?;
            return Ok(Expr::Neg(Box::new(sub_expr)));
        }

        // Buscar operador al nivel de paréntesis correcto
        let mut depth = 0;
        let mut op_pos = None;
        let mut op_char = ' ';
        
        let chars: Vec<char> = inner.chars().collect();
        for (i, &c) in chars.iter().enumerate() {
            match c {
                '(' => depth += 1,
                ')' => depth -= 1,
                '+' | '-' | '*' if depth == 0 && i > 0 => {
                    // Preferir + y - sobre * (menor precedencia primero)
                    if op_pos.is_none() || c == '+' || c == '-' {
                        // Verificar que no es parte de un número negativo
                        if c != '-' || !matches!(chars.get(i.wrapping_sub(1)), Some(' ') | None) {
                            op_pos = Some(i);
                            op_char = c;
                        }
                    }
                }
                _ => {}
            }
        }

        if let Some(pos) = op_pos {
            let left_str: String = inner[..pos].trim().to_string();
            let right_str: String = inner[pos + 1..].trim().to_string();
            
            let left = Expr::parse(&left_str)?;
            let right = Expr::parse(&right_str)?;

            return match op_char {
                '+' => Ok(Expr::Add(Box::new(left), Box::new(right))),
                '-' => Ok(Expr::Sub(Box::new(left), Box::new(right))),
                '*' => Ok(Expr::Mul(Box::new(left), Box::new(right))),
                _ => Err(format!("unknown operator: {}", op_char)),
            };
        }

        Err(format!("cannot parse: {}", input))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_eval_basic() {
        assert_eq!(Expr::Num(42.0).eval(), 42.0);
        assert_eq!(
            Expr::Add(
                Box::new(Expr::Num(2.0)),
                Box::new(Expr::Num(3.0))
            ).eval(),
            5.0
        );
    }

    #[test]
    fn test_simplify_identity() {
        let expr = Expr::Add(
            Box::new(Expr::Num(5.0)),
            Box::new(Expr::Num(0.0)),
        );
        assert_eq!(expr.simplify(), Expr::Num(5.0));
    }

    #[test]
    fn test_parse_basic() {
        assert_eq!(Expr::parse("42").unwrap(), Expr::Num(42.0));
        assert_eq!(
            Expr::parse("(1 + 2)").unwrap(),
            Expr::Add(Box::new(Expr::Num(1.0)), Box::new(Expr::Num(2.0)))
        );
    }
}
```

### tests/properties.rs — Property tests completos

```rust
use math_expr::Expr;
use proptest::prelude::*;

// ============================================================
// ESTRATEGIA: Generar expresiones aleatorias
// ============================================================

fn expr_strategy() -> impl Strategy<Value = Expr> {
    let leaf = prop_oneof![
        // Números enteros pequeños (más fáciles de razonar)
        (-100..=100i32).prop_map(|n| Expr::Num(n as f64)),
        // Cero y uno (casos edge)
        Just(Expr::Num(0.0)),
        Just(Expr::Num(1.0)),
        Just(Expr::Num(-1.0)),
    ];

    leaf.prop_recursive(
        4,   // profundidad máxima
        32,  // máximo de nodos
        3,   // items por recursión
        |inner| {
            prop_oneof![
                // Negación
                inner.clone().prop_map(|e| Expr::Neg(Box::new(e))),
                // Suma
                (inner.clone(), inner.clone())
                    .prop_map(|(a, b)| Expr::Add(Box::new(a), Box::new(b))),
                // Resta
                (inner.clone(), inner.clone())
                    .prop_map(|(a, b)| Expr::Sub(Box::new(a), Box::new(b))),
                // Multiplicación
                (inner.clone(), inner)
                    .prop_map(|(a, b)| Expr::Mul(Box::new(a), Box::new(b))),
            ]
        },
    )
}

// ============================================================
// PROPIEDAD 1: simplify no cambia el resultado de eval
// ============================================================

proptest! {
    #![proptest_config(ProptestConfig::with_cases(1000))]

    #[test]
    fn simplify_preserves_eval(expr in expr_strategy()) {
        let original_value = expr.eval();
        let simplified = expr.simplify();
        let simplified_value = simplified.eval();

        // Manejar NaN (puede ocurrir con 0.0 * inf si se genera)
        if original_value.is_nan() {
            prop_assert!(simplified_value.is_nan());
        } else if original_value.is_infinite() {
            prop_assert_eq!(original_value, simplified_value);
        } else {
            // Tolerancia por errores de punto flotante
            let diff = (original_value - simplified_value).abs();
            prop_assert!(
                diff < 1e-10,
                "simplify cambió el valor: {} → {} (diff={})\nexpr: {:?}\nsimplified: {:?}",
                original_value, simplified_value, diff,
                expr, simplified
            );
        }
    }
}

// ============================================================
// PROPIEDAD 2: simplify reduce o mantiene la cantidad de nodos
// ============================================================

proptest! {
    #[test]
    fn simplify_reduces_nodes(expr in expr_strategy()) {
        let original_nodes = expr.node_count();
        let simplified_nodes = expr.simplify().node_count();

        prop_assert!(
            simplified_nodes <= original_nodes,
            "simplify AUMENTÓ nodos: {} → {}\nexpr: {:?}",
            original_nodes, simplified_nodes, expr
        );
    }
}

// ============================================================
// PROPIEDAD 3: simplify es idempotente
// ============================================================

proptest! {
    #[test]
    fn simplify_is_idempotent(expr in expr_strategy()) {
        let once = expr.simplify();
        let twice = once.simplify();

        // Simplificar dos veces debe dar el mismo resultado
        prop_assert_eq!(
            once.eval(), twice.eval(),
            "simplify no es idempotente:\nonce:  {:?}\ntwice: {:?}",
            once, twice
        );

        prop_assert_eq!(
            once.node_count(), twice.node_count(),
            "segunda simplificación redujo más nodos"
        );
    }
}

// ============================================================
// PROPIEDAD 4: doble negación se cancela
// ============================================================

proptest! {
    #[test]
    fn double_negation_cancels(expr in expr_strategy()) {
        let double_neg = Expr::Neg(Box::new(Expr::Neg(Box::new(expr.clone()))));
        let simplified = double_neg.simplify();

        let original_value = expr.eval();
        let simplified_value = simplified.eval();

        if !original_value.is_nan() {
            let diff = (original_value - simplified_value).abs();
            prop_assert!(
                diff < 1e-10,
                "doble negación no se canceló: {} vs {}",
                original_value, simplified_value
            );
        }
    }
}

// ============================================================
// PROPIEDAD 5: x + 0 = x (identidad de la suma)
// ============================================================

proptest! {
    #[test]
    fn add_zero_identity(expr in expr_strategy()) {
        let with_zero = Expr::Add(
            Box::new(expr.clone()),
            Box::new(Expr::Num(0.0)),
        );
        let simplified = with_zero.simplify();

        let original_value = expr.eval();
        let simplified_value = simplified.eval();

        if !original_value.is_nan() {
            let diff = (original_value - simplified_value).abs();
            prop_assert!(
                diff < 1e-10,
                "x + 0 no se simplificó a x"
            );
        }
    }
}

// ============================================================
// PROPIEDAD 6: x * 1 = x (identidad de la multiplicación)
// ============================================================

proptest! {
    #[test]
    fn mul_one_identity(expr in expr_strategy()) {
        let with_one = Expr::Mul(
            Box::new(expr.clone()),
            Box::new(Expr::Num(1.0)),
        );
        let simplified = with_one.simplify();

        let original_value = expr.eval();
        let simplified_value = simplified.eval();

        if !original_value.is_nan() {
            let diff = (original_value - simplified_value).abs();
            prop_assert!(
                diff < 1e-10,
                "x * 1 no se simplificó a x"
            );
        }
    }
}

// ============================================================
// PROPIEDAD 7: x * 0 = 0 (absorbente de la multiplicación)
// ============================================================

proptest! {
    #[test]
    fn mul_zero_absorbs(expr in expr_strategy()) {
        let with_zero = Expr::Mul(
            Box::new(expr.clone()),
            Box::new(Expr::Num(0.0)),
        );
        let simplified = with_zero.simplify();
        let simplified_value = simplified.eval();

        // Solo aplicar cuando eval(expr) es finito
        let original_value = expr.eval();
        if original_value.is_finite() {
            prop_assert!(
                (simplified_value - 0.0).abs() < 1e-10,
                "x * 0 no se simplificó a 0: resultado = {}",
                simplified_value
            );
        }
    }
}

// ============================================================
// PROPIEDAD 8: round-trip to_string → parse
// ============================================================

proptest! {
    #[test]
    fn roundtrip_string_parse(expr in expr_strategy()) {
        let stringified = expr.to_string_explicit();

        // Intentar parsear de vuelta
        if let Ok(parsed) = Expr::parse(&stringified) {
            let original_value = expr.eval();
            let parsed_value = parsed.eval();

            if !original_value.is_nan() && original_value.is_finite() {
                let diff = (original_value - parsed_value).abs();
                prop_assert!(
                    diff < 1e-6,
                    "round-trip falló:\n  original: {:?} = {}\n  string:   {}\n  parsed:   {:?} = {}",
                    expr, original_value,
                    stringified,
                    parsed, parsed_value
                );
            }
        }
        // Si parse falla, es OK — nuestro parser simplificado
        // no maneja todos los casos
    }
}

// ============================================================
// PROPIEDAD 9: eval produce números (no panics)
// ============================================================

proptest! {
    #[test]
    fn eval_never_panics(expr in expr_strategy()) {
        // Esta propiedad verifica que eval() nunca hace panic
        // Si llega al prop_assert!, no hubo panic
        let result = expr.eval();
        // El resultado puede ser NaN o Inf, pero no debe hacer panic
        prop_assert!(result.is_nan() || result.is_finite() || result.is_infinite());
    }
}

// ============================================================
// PROPIEDAD 10: node_count es consistente
// ============================================================

proptest! {
    #[test]
    fn node_count_positive(expr in expr_strategy()) {
        prop_assert!(expr.node_count() >= 1);

        match &expr {
            Expr::Num(_) => {
                prop_assert_eq!(expr.node_count(), 1);
            }
            Expr::Neg(inner) => {
                prop_assert_eq!(expr.node_count(), 1 + inner.node_count());
            }
            Expr::Add(a, b) | Expr::Sub(a, b) | Expr::Mul(a, b) => {
                prop_assert_eq!(
                    expr.node_count(),
                    1 + a.node_count() + b.node_count()
                );
            }
        }
    }
}
```

---

## 25. Programa de práctica

### Proyecto: `interval_set` — conjunto de intervalos con property tests

Implementa una estructura que representa un conjunto de intervalos numéricos con unión, intersección y diferencia, verificando propiedades algebraicas con proptest.

### Código fuente: src/lib.rs

```rust
/// Un intervalo cerrado [lo, hi]
#[derive(Debug, Clone, PartialEq)]
pub struct Interval {
    pub lo: i64,
    pub hi: i64,
}

impl Interval {
    pub fn new(lo: i64, hi: i64) -> Option<Self> {
        if lo <= hi {
            Some(Interval { lo, hi })
        } else {
            None
        }
    }

    pub fn contains(&self, x: i64) -> bool {
        x >= self.lo && x <= self.hi
    }

    pub fn len(&self) -> u64 {
        (self.hi - self.lo) as u64 + 1
    }

    pub fn overlaps(&self, other: &Interval) -> bool {
        self.lo <= other.hi && other.lo <= self.hi
    }

    pub fn merge(&self, other: &Interval) -> Option<Interval> {
        if self.overlaps(other) || self.hi + 1 == other.lo || other.hi + 1 == self.lo {
            Some(Interval {
                lo: self.lo.min(other.lo),
                hi: self.hi.max(other.hi),
            })
        } else {
            None
        }
    }

    pub fn intersect(&self, other: &Interval) -> Option<Interval> {
        let lo = self.lo.max(other.lo);
        let hi = self.hi.min(other.hi);
        if lo <= hi {
            Some(Interval { lo, hi })
        } else {
            None
        }
    }
}

/// Conjunto de intervalos disjuntos y ordenados
#[derive(Debug, Clone, PartialEq)]
pub struct IntervalSet {
    // Invariante: intervalos disjuntos, ordenados por lo, no adyacentes
    intervals: Vec<Interval>,
}

impl IntervalSet {
    pub fn new() -> Self {
        IntervalSet { intervals: Vec::new() }
    }

    pub fn from_interval(interval: Interval) -> Self {
        IntervalSet { intervals: vec![interval] }
    }

    pub fn is_empty(&self) -> bool {
        self.intervals.is_empty()
    }

    pub fn intervals(&self) -> &[Interval] {
        &self.intervals
    }

    pub fn contains(&self, x: i64) -> bool {
        self.intervals.iter().any(|iv| iv.contains(x))
    }

    /// Insertar un intervalo, manteniendo invariantes
    pub fn insert(&mut self, new: Interval) {
        let mut merged = new;
        let mut to_remove = Vec::new();

        for (i, existing) in self.intervals.iter().enumerate() {
            if let Some(m) = merged.merge(existing) {
                merged = m;
                to_remove.push(i);
            }
        }

        // Remover los que se fusionaron (en orden reverso)
        for &i in to_remove.iter().rev() {
            self.intervals.remove(i);
        }

        // Insertar el intervalo fusionado en la posición correcta
        let pos = self.intervals
            .iter()
            .position(|iv| iv.lo > merged.lo)
            .unwrap_or(self.intervals.len());
        self.intervals.insert(pos, merged);
    }

    /// Unión de dos conjuntos
    pub fn union(&self, other: &IntervalSet) -> IntervalSet {
        let mut result = self.clone();
        for iv in &other.intervals {
            result.insert(iv.clone());
        }
        result
    }

    /// Intersección de dos conjuntos
    pub fn intersection(&self, other: &IntervalSet) -> IntervalSet {
        let mut result = IntervalSet::new();
        for a in &self.intervals {
            for b in &other.intervals {
                if let Some(inter) = a.intersect(b) {
                    result.insert(inter);
                }
            }
        }
        result
    }

    /// Complemento respecto a un intervalo universal
    pub fn complement(&self, universe: &Interval) -> IntervalSet {
        let mut result = IntervalSet::new();
        let mut current = universe.lo;

        for iv in &self.intervals {
            if iv.lo > current {
                if let Some(gap) = Interval::new(current, iv.lo - 1) {
                    result.insert(gap);
                }
            }
            current = iv.hi + 1;
        }

        if current <= universe.hi {
            if let Some(gap) = Interval::new(current, universe.hi) {
                result.insert(gap);
            }
        }

        result
    }

    /// Verificar invariantes internas
    pub fn check_invariants(&self) -> Result<(), String> {
        for i in 0..self.intervals.len() {
            // Cada intervalo es válido
            if self.intervals[i].lo > self.intervals[i].hi {
                return Err(format!(
                    "Intervalo inválido en posición {}: {:?}",
                    i, self.intervals[i]
                ));
            }

            // Ordenados y no solapados ni adyacentes
            if i > 0 {
                let prev = &self.intervals[i - 1];
                let curr = &self.intervals[i];
                if prev.hi >= curr.lo {
                    return Err(format!(
                        "Solapamiento: {:?} y {:?}",
                        prev, curr
                    ));
                }
                if prev.hi + 1 == curr.lo {
                    return Err(format!(
                        "Adyacentes no fusionados: {:?} y {:?}",
                        prev, curr
                    ));
                }
            }
        }
        Ok(())
    }
}
```

### Tests con proptest: tests/properties.rs

```rust
use interval_set::{Interval, IntervalSet};
use proptest::prelude::*;

// ── Estrategias ──────────────────────────────────────────────

fn interval_strategy() -> impl Strategy<Value = Interval> {
    (-500..=500i64, -500..=500i64)
        .prop_map(|(a, b)| {
            let lo = a.min(b);
            let hi = a.max(b);
            Interval::new(lo, hi).unwrap()
        })
}

fn interval_set_strategy() -> impl Strategy<Value = IntervalSet> {
    proptest::collection::vec(interval_strategy(), 0..8)
        .prop_map(|intervals| {
            let mut set = IntervalSet::new();
            for iv in intervals {
                set.insert(iv);
            }
            set
        })
}

fn universe() -> Interval {
    Interval::new(-1000, 1000).unwrap()
}

// ── Propiedad 1: invariantes siempre se mantienen ────────────

proptest! {
    #![proptest_config(ProptestConfig::with_cases(500))]

    #[test]
    fn invariants_after_insert(
        intervals in proptest::collection::vec(interval_strategy(), 1..15)
    ) {
        let mut set = IntervalSet::new();
        for iv in intervals {
            set.insert(iv);
            prop_assert!(
                set.check_invariants().is_ok(),
                "Invariante rota: {:?}", set
            );
        }
    }
}

// ── Propiedad 2: unión es conmutativa ────────────────────────

proptest! {
    #[test]
    fn union_commutative(
        a in interval_set_strategy(),
        b in interval_set_strategy()
    ) {
        let ab = a.union(&b);
        let ba = b.union(&a);
        prop_assert_eq!(ab, ba, "unión no es conmutativa");
    }
}

// ── Propiedad 3: intersección es conmutativa ─────────────────

proptest! {
    #[test]
    fn intersection_commutative(
        a in interval_set_strategy(),
        b in interval_set_strategy()
    ) {
        let ab = a.intersection(&b);
        let ba = b.intersection(&a);
        prop_assert_eq!(ab, ba, "intersección no es conmutativa");
    }
}

// ── Propiedad 4: A ∪ A = A (idempotencia) ───────────────────

proptest! {
    #[test]
    fn union_idempotent(a in interval_set_strategy()) {
        let aa = a.union(&a);
        prop_assert_eq!(aa, a, "A ∪ A ≠ A");
    }
}

// ── Propiedad 5: A ∩ A = A (idempotencia) ───────────────────

proptest! {
    #[test]
    fn intersection_idempotent(a in interval_set_strategy()) {
        let aa = a.intersection(&a);
        prop_assert_eq!(aa, a, "A ∩ A ≠ A");
    }
}

// ── Propiedad 6: A ∩ B ⊆ A y A ∩ B ⊆ B ─────────────────────

proptest! {
    #[test]
    fn intersection_is_subset(
        a in interval_set_strategy(),
        b in interval_set_strategy(),
        x in -500..=500i64
    ) {
        let inter = a.intersection(&b);
        if inter.contains(x) {
            prop_assert!(a.contains(x), "x={} en A∩B pero no en A", x);
            prop_assert!(b.contains(x), "x={} en A∩B pero no en B", x);
        }
    }
}

// ── Propiedad 7: A ⊆ A ∪ B y B ⊆ A ∪ B ─────────────────────

proptest! {
    #[test]
    fn union_is_superset(
        a in interval_set_strategy(),
        b in interval_set_strategy(),
        x in -500..=500i64
    ) {
        let union = a.union(&b);
        if a.contains(x) {
            prop_assert!(union.contains(x), "x={} en A pero no en A∪B", x);
        }
        if b.contains(x) {
            prop_assert!(union.contains(x), "x={} en B pero no en A∪B", x);
        }
    }
}

// ── Propiedad 8: complemento − A ∪ complemento(A) = universo ─

proptest! {
    #[test]
    fn complement_covers_universe(
        a in interval_set_strategy(),
        x in -1000..=1000i64
    ) {
        let u = universe();
        let comp = a.complement(&u);
        
        if u.contains(x) {
            // x está en A o en el complemento de A (o ambos si hay bug)
            let in_a = a.contains(x);
            let in_comp = comp.contains(x);
            prop_assert!(
                in_a ^ in_comp,
                "x={}: in_a={}, in_comp={} (deben ser exactamente uno)",
                x, in_a, in_comp
            );
        }
    }
}

// ── Propiedad 9: insert es idempotente ───────────────────────

proptest! {
    #[test]
    fn insert_idempotent(
        base in interval_set_strategy(),
        iv in interval_strategy()
    ) {
        let mut once = base.clone();
        once.insert(iv.clone());
        
        let mut twice = once.clone();
        twice.insert(iv);
        
        prop_assert_eq!(once, twice, "insertar dos veces cambió el set");
    }
}

// ── Propiedad 10: contiene todos los puntos insertados ───────

proptest! {
    #[test]
    fn contains_inserted_points(
        intervals in proptest::collection::vec(interval_strategy(), 1..5),
        test_offset in 0..100i64
    ) {
        let mut set = IntervalSet::new();
        for iv in &intervals {
            set.insert(iv.clone());
        }

        // Cada punto de cada intervalo debe estar en el set
        for iv in &intervals {
            let test_point = iv.lo + (test_offset % iv.len() as i64);
            if test_point <= iv.hi {
                prop_assert!(
                    set.contains(test_point),
                    "punto {} del intervalo {:?} no encontrado en {:?}",
                    test_point, iv, set
                );
            }
        }
    }
}
```

### Ejecución y verificación

```bash
# Correr todos los property tests
cargo test

# Más casos para mayor confianza
PROPTEST_CASES=5000 cargo test

# Solo property tests
cargo test --test properties

# Ver output detallado si falla
cargo test --test properties -- --nocapture
```

---

## 26. Ejercicios

### Ejercicio 1: Estrategias para un sistema de coordenadas

**Objetivo**: Practicar `prop_compose!`, `prop_flat_map` y estrategias custom.

**Contexto**: tienes estos tipos:

```rust
#[derive(Debug, Clone, PartialEq)]
struct GeoPoint {
    lat: f64,   // -90.0 ..= 90.0
    lon: f64,   // -180.0 ..= 180.0
}

#[derive(Debug, Clone)]
struct GeoBox {
    sw: GeoPoint,  // esquina suroeste
    ne: GeoPoint,  // esquina noreste (lat > sw.lat, lon > sw.lon)
}
```

**Tareas**:

**a)** Implementa `geo_point_strategy()` que genere puntos con coordenadas válidas.

**b)** Implementa `geo_box_strategy()` usando `prop_flat_map` para que `ne` siempre sea estrictamente mayor que `sw` en ambas coordenadas.

**c)** Implementa `point_in_box_strategy()` que genere una tupla `(GeoBox, GeoPoint)` donde el punto siempre está dentro del box.

**d)** Escribe estas propiedades:
- Un punto generado con `point_in_box_strategy` siempre está contenido en su box
- Un GeoBox siempre tiene área > 0
- Dos GeoBoxes que se solapan tienen intersección no vacía

---

### Ejercicio 2: Testing de un codec Base64

**Objetivo**: Practicar round-trip properties y regex strategies.

**Tareas**:

**a)** Implementa funciones `base64_encode(data: &[u8]) -> String` y `base64_decode(s: &str) -> Result<Vec<u8>, String>`.

**b)** Escribe estas propiedades con proptest:
- **Round-trip**: `base64_decode(base64_encode(data)) == Ok(data)` para cualquier `Vec<u8>`
- **Formato**: el output de encode solo contiene caracteres `[A-Za-z0-9+/=]`
- **Longitud**: el output de encode tiene longitud múltiplo de 4
- **Decode de basura**: `base64_decode` de strings que no matchean el regex `[A-Za-z0-9+/=]*` retorna `Err`

**c)** Usa regex strategy para generar strings Base64 válidas y verifica que `decode` siempre produce `Ok`.

---

### Ejercicio 3: Máquina de estado de una cola de prioridad

**Objetivo**: Practicar testing de máquinas de estado con oracle.

**Tareas**:

**a)** Define este enum de operaciones:

```rust
enum HeapOp {
    Push(i32),
    Pop,
    Peek,
    Len,
}
```

**b)** Genera secuencias aleatorias de 1 a 100 operaciones.

**c)** Ejecuta cada secuencia en paralelo sobre:
- `std::collections::BinaryHeap` (oracle)
- Tu propia implementación de min-heap (SUT)

**d)** Verifica que ambas producen los mismos resultados para cada operación.

**e)** Añade una propiedad adicional: después de cualquier secuencia de operaciones, pop repetido extrae los elementos en orden.

---

### Ejercicio 4: Propiedades de serialización JSON

**Objetivo**: Integrar proptest con serde_json.

**Tareas**:

**a)** Define un struct `Config` con campos de varios tipos (`String`, `u32`, `bool`, `Vec<String>`, `Option<f64>`) y deriva `Serialize`, `Deserialize`, `Debug`, `PartialEq`.

**b)** Implementa `Arbitrary` para `Config`.

**c)** Escribe estas propiedades:
- **Round-trip**: `serde_json::from_str(&serde_json::to_string(&config)?) == Ok(config)`
- **Pretty vs compact**: `from_str(to_string_pretty(&c))` y `from_str(to_string(&c))` producen el mismo Config
- **Idempotencia de serialización**: `to_string(from_str(to_string(&c))?)` == `to_string(&c)`
- **El JSON producido es válido**: `serde_json::Value::from_str(&to_string(&c))` siempre es `Ok`

**d)** Usa `ProptestConfig` con 2000 casos y verifica que todos pasan.

---

## Navegación

- **Anterior**: [T01 - Concepto de Property-Based Testing](../T01_Concepto/README.md)
- **Siguiente**: [T03 - Diseñar propiedades](../T03_Disenar_propiedades/README.md)

---

> **Sección 3: Property-Based Testing** — Tópico 2 de 4 completado
>
> - T01: Concepto — generación aleatoria, propiedades invariantes, shrinking ✓
> - T02: proptest — proptest!, any::\<T\>(), estrategias, ProptestConfig, regex ✓
> - T03: Diseñar propiedades (siguiente)
> - T04: proptest vs quickcheck
