# T04 - proptest vs quickcheck: dos filosofías de Property-Based Testing

## Índice

1. [Panorama: dos crates, dos filosofías](#1-panorama-dos-crates-dos-filosofías)
2. [Historia y linaje](#2-historia-y-linaje)
3. [Instalación y setup comparado](#3-instalación-y-setup-comparado)
4. [Sintaxis básica lado a lado](#4-sintaxis-básica-lado-a-lado)
5. [Generación de valores: Arbitrary vs Strategy](#5-generación-de-valores-arbitrary-vs-strategy)
6. [Generadores built-in: cobertura de tipos](#6-generadores-built-in-cobertura-de-tipos)
7. [Composición de generadores](#7-composición-de-generadores)
8. [Shrinking: la diferencia fundamental](#8-shrinking-la-diferencia-fundamental)
9. [Shrinking determinístico (proptest) en detalle](#9-shrinking-determinístico-proptest-en-detalle)
10. [Shrinking basado en Arbitrary (quickcheck) en detalle](#10-shrinking-basado-en-arbitrary-quickcheck-en-detalle)
11. [Impacto práctico de las diferencias de shrinking](#11-impacto-práctico-de-las-diferencias-de-shrinking)
12. [Configuración y control del runner](#12-configuración-y-control-del-runner)
13. [Archivos de regresión](#13-archivos-de-regresión)
14. [Generación de strings y regex](#14-generación-de-strings-y-regex)
15. [Estructuras recursivas](#15-estructuras-recursivas)
16. [Testing de máquinas de estado](#16-testing-de-máquinas-de-estado)
17. [Integración con el ecosistema](#17-integración-con-el-ecosistema)
18. [Rendimiento: velocidad de generación y shrinking](#18-rendimiento-velocidad-de-generación-y-shrinking)
19. [Ergonomía y curva de aprendizaje](#19-ergonomía-y-curva-de-aprendizaje)
20. [Tabla comparativa exhaustiva](#20-tabla-comparativa-exhaustiva)
21. [Cuándo usar cada uno](#21-cuándo-usar-cada-uno)
22. [Migrar de quickcheck a proptest](#22-migrar-de-quickcheck-a-proptest)
23. [Alternativas: bolero y otros](#23-alternativas-bolero-y-otros)
24. [Comparación con PBT en C y Go](#24-comparación-con-pbt-en-c-y-go)
25. [Errores comunes al elegir framework](#25-errores-comunes-al-elegir-framework)
26. [Ejemplo completo: misma propiedad en ambos frameworks](#26-ejemplo-completo-misma-propiedad-en-ambos-frameworks)
27. [Programa de práctica](#27-programa-de-práctica)
28. [Ejercicios](#28-ejercicios)

---

## 1. Panorama: dos crates, dos filosofías

Rust tiene dos frameworks principales de property-based testing: **proptest** y **quickcheck**. Aunque ambos generan inputs aleatorios y verifican propiedades, difieren profundamente en su diseño interno.

```
┌─────────────────────────────────────────────────────────────────┐
│              DOS FILOSOFÍAS DE PBT EN RUST                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  quickcheck                        proptest                     │
│  ──────────                        ───────                      │
│  Linaje: QuickCheck (Haskell)      Linaje: Hypothesis (Python)  │
│  Filosofía: port fiel del original Filosofía: diseño desde cero │
│                                    para Rust                    │
│  Modelo: generación + shrinking    Modelo: estrategias como     │
│          acoplados en Arbitrary             valores componibles │
│                                                                 │
│  Shrinking: tipo genera sus        Shrinking: basado en la      │
│    propios candidatos reducidos     estructura del valor (árbol) │
│    (no determinístico)              (determinístico)             │
│                                                                 │
│  API: simple, minimalista          API: rica, funcional          │
│  Macros: quickcheck!               Macros: proptest!,            │
│                                    prop_compose!, prop_oneof!    │
│                                                                 │
│  Regex strategies: NO              Regex strategies: SÍ          │
│  Archivos de regresión: NO         Archivos de regresión: SÍ     │
│                                                                 │
│  Madurez: estable, bajo manteni-   Madurez: activo, evoluciona   │
│           miento                                                 │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Historia y linaje

### Árbol genealógico del PBT

```
  1999 ── QuickCheck (Haskell) ─── Koen Claessen & John Hughes
           │
           ├── 2006 ── ScalaCheck (Scala)
           │
           ├── 2009 ── QuickCheck (Erlang) ─── John Hughes (Quviq)
           │
           ├── 2014 ── quickcheck (Rust) ─── Andrew Gallant (BurntSushi)
           │            │
           │            └── Port directo de la filosofía QuickCheck:
           │                Arbitrary trait, Gen, Shrink como método
           │                del tipo generado
           │
           ├── 2016 ── Hypothesis (Python) ─── David MacIver
           │            │
           │            └── Innovación: estrategias basadas en
           │                "bytes → valor", shrinking determinístico,
           │                base de datos de ejemplos
           │
           └── 2017 ── proptest (Rust) ─── Jason Lingle
                        │
                        └── Inspirado en Hypothesis:
                            Estrategias composables, shrinking
                            determinístico, archivos de regresión,
                            regex strategies
```

### quickcheck en Rust

- **Autor**: Andrew Gallant (BurntSushi, creador de `ripgrep`)
- **Primera versión**: 2014
- **Filosofía**: ser un port fiel de QuickCheck de Haskell
- **Estado actual**: mantenimiento mínimo, estable pero sin innovación
- **Descargas**: ~400K/mes (sigue siendo popular por inercia y simplicidad)

### proptest en Rust

- **Autor**: Jason Lingle
- **Primera versión**: 2017
- **Filosofía**: tomar lo mejor de Hypothesis y adaptarlo a Rust
- **Estado actual**: desarrollo activo, innovando
- **Descargas**: ~600K/mes (dominante en nuevos proyectos)

---

## 3. Instalación y setup comparado

### quickcheck

```toml
# Cargo.toml
[dev-dependencies]
quickcheck = "1"
quickcheck_macros = "1"  # para el atributo #[quickcheck]
```

```rust
// Importación
use quickcheck::{quickcheck, Arbitrary, Gen};
// O con la macro de atributo:
use quickcheck_macros::quickcheck;
```

### proptest

```toml
# Cargo.toml
[dev-dependencies]
proptest = "1"
```

```rust
// Importación
use proptest::prelude::*;
```

### Comparación de setup

```
┌───────────────┬───────────────────────────┬──────────────────────┐
│               │ quickcheck                │ proptest             │
├───────────────┼───────────────────────────┼──────────────────────┤
│ Crates        │ 2 (quickcheck +           │ 1 (proptest)         │
│ necesarios    │ quickcheck_macros)         │                      │
│               │                           │                      │
│ Líneas en     │ 2                         │ 1                    │
│ Cargo.toml    │                           │                      │
│               │                           │                      │
│ Import        │ use quickcheck::*         │ use proptest::       │
│               │ use quickcheck_macros::*  │   prelude::*         │
│               │                           │                      │
│ Dependencias  │ ~5 crates transitivos     │ ~15 crates           │
│ transitivas   │ (más ligero)              │ transitivos          │
│               │                           │ (incluye regex)      │
│               │                           │                      │
│ Tiempo de     │ ~3s                       │ ~8s                  │
│ compilación   │ (más rápido)              │ (más lento)          │
│ (primera vez) │                           │                      │
└───────────────┴───────────────────────────┴──────────────────────┘
```

---

## 4. Sintaxis básica lado a lado

### Test simple: verificar conmutatividad de la suma

```rust
// ══════════════════════════════════════════════════════════════
// quickcheck
// ══════════════════════════════════════════════════════════════

// Forma 1: macro quickcheck!
#[cfg(test)]
mod tests_qc {
    use quickcheck::quickcheck;
    
    quickcheck! {
        fn prop_add_commutative(a: i32, b: i32) -> bool {
            a.wrapping_add(b) == b.wrapping_add(a)
        }
    }
}

// Forma 2: atributo #[quickcheck]
#[cfg(test)]
mod tests_qc_attr {
    use quickcheck_macros::quickcheck;
    
    #[quickcheck]
    fn prop_add_commutative(a: i32, b: i32) -> bool {
        a.wrapping_add(b) == b.wrapping_add(a)
    }
}

// Forma 3: con TestResult (para filtrar inputs)
#[cfg(test)]
mod tests_qc_result {
    use quickcheck::{quickcheck, TestResult};
    
    quickcheck! {
        fn prop_division(a: i32, b: i32) -> TestResult {
            if b == 0 {
                return TestResult::discard();  // descartar
            }
            TestResult::from_bool(a / b * b + a % b == a)
        }
    }
}
```

```rust
// ══════════════════════════════════════════════════════════════
// proptest
// ══════════════════════════════════════════════════════════════

// Forma principal: macro proptest!
#[cfg(test)]
mod tests_pt {
    use proptest::prelude::*;
    
    proptest! {
        #[test]
        fn prop_add_commutative(a in any::<i32>(), b in any::<i32>()) {
            prop_assert_eq!(a.wrapping_add(b), b.wrapping_add(a));
        }
    }
}

// Con filtrado de inputs
#[cfg(test)]
mod tests_pt_filter {
    use proptest::prelude::*;
    
    proptest! {
        #[test]
        fn prop_division(a in any::<i32>(), b in any::<i32>()) {
            prop_assume!(b != 0);
            prop_assume!(!(a == i32::MIN && b == -1));
            prop_assert_eq!(a / b * b + a % b, a);
        }
    }
}
```

### Diferencias sintácticas clave

```
┌─────────────────────────┬──────────────────────┬────────────────────────┐
│ Concepto                │ quickcheck           │ proptest               │
├─────────────────────────┼──────────────────────┼────────────────────────┤
│ Definir test            │ fn name(x: T) -> bool│ fn name(x in strategy) │
│                         │ (retorna bool)       │ { prop_assert!(...) }  │
│                         │                      │                        │
│ Indicar fallo           │ return false          │ prop_assert!(...)?     │
│                         │ o panic              │ (retorna Err)          │
│                         │                      │                        │
│ Filtrar inputs          │ TestResult::discard() │ prop_assume!(cond)     │
│                         │                      │                        │
│ Especificar estrategia  │ Solo por tipo         │ x in 0..100i32         │
│                         │ (Arbitrary)          │ x in "[a-z]+"          │
│                         │                      │ x in any::<T>()        │
│                         │                      │                        │
│ Atributo #[test]        │ Implícito en macro    │ Explícito dentro del   │
│                         │ o #[quickcheck]      │ bloque proptest! {}    │
│                         │                      │                        │
│ Múltiples tests         │ Bloque quickcheck!{} │ Bloque proptest!{}     │
│ en un bloque            │                      │                        │
│                         │                      │                        │
│ Mensajes de error       │ Solo "test failed"   │ Mensaje formateado con │
│                         │ con valor que falló  │ valores y diff         │
└─────────────────────────┴──────────────────────┴────────────────────────┘
```

---

## 5. Generación de valores: Arbitrary vs Strategy

Esta es la diferencia arquitectónica fundamental entre los dos frameworks.

### quickcheck: trait Arbitrary

En quickcheck, la generación de valores se centra en el trait `Arbitrary`:

```rust
// Definición simplificada del trait Arbitrary en quickcheck
pub trait Arbitrary: Clone + Send + 'static {
    fn arbitrary(g: &mut Gen) -> Self;
    
    // Shrinking: retorna un iterador de "versiones más pequeñas"
    fn shrink(&self) -> Box<dyn Iterator<Item = Self>> {
        Box::new(std::iter::empty())  // por defecto: no shrink
    }
}
```

```
  quickcheck: TODO está en Arbitrary
  
  ┌─────────────────────────────────────────┐
  │           trait Arbitrary               │
  │  ┌──────────────────┐                   │
  │  │  arbitrary(gen)  │ ← generación      │
  │  │  → Self          │                   │
  │  └──────────────────┘                   │
  │  ┌──────────────────┐                   │
  │  │  shrink(&self)   │ ← reducción       │
  │  │  → Iterator<Self>│                   │
  │  └──────────────────┘                   │
  │                                         │
  │  Generación y shrinking están ACOPLADOS │
  │  en el mismo trait, en el mismo tipo.   │
  │                                         │
  │  Para generar un i32:                   │
  │    i32::arbitrary(gen) → algún i32      │
  │    valor.shrink() → iter de i32 menores │
  └─────────────────────────────────────────┘
```

### proptest: trait Strategy

En proptest, la generación se centra en el trait `Strategy`:

```rust
// Definición simplificada del trait Strategy en proptest
pub trait Strategy: fmt::Debug {
    type Value: fmt::Debug;
    
    fn new_tree(
        &self,
        runner: &mut TestRunner,
    ) -> Result<Self::Tree, Reason>;
    
    // Tree tiene métodos current() y simplify() para shrinking
    type Tree: ValueTree<Value = Self::Value>;
}

pub trait ValueTree {
    type Value: fmt::Debug;
    
    fn current(&self) -> Self::Value;  // valor actual
    fn simplify(&mut self) -> bool;    // intentar reducir
    fn complicate(&mut self) -> bool;  // deshacer última reducción
}
```

```
  proptest: Strategy y ValueTree están SEPARADOS del tipo
  
  ┌─────────────────────────────────────────────────────────┐
  │                                                         │
  │  Strategy ────────────→ ValueTree ────────→ Value       │
  │  (plan de                (árbol de           (valor      │
  │   generación)             reducción)          concreto)  │
  │                                                         │
  │  0..100i32  ──new_tree──→  BinarySearch   ──current──→ 42│
  │  (estrategia)              (puede shrink                │
  │                             determinístico)             │
  │                                                         │
  │  Generación y shrinking están DESACOPLADOS del tipo.    │
  │  Diferentes estrategias para el MISMO tipo pueden tener │
  │  diferentes formas de shrinking.                        │
  │                                                         │
  │  Para generar un i32:                                   │
  │    (0..100).new_tree(runner) → ValueTree                │
  │    tree.current() → 42                                  │
  │    tree.simplify() → true (ahora current() → 21)       │
  │    tree.simplify() → true (ahora current() → 10)       │
  │    ...                                                  │
  └─────────────────────────────────────────────────────────┘
```

### Consecuencias de esta diferencia

```
┌───────────────────────────────┬──────────────────────────────────┐
│ quickcheck (Arbitrary)        │ proptest (Strategy)              │
├───────────────────────────────┼──────────────────────────────────┤
│ Un tipo = una forma de generar│ Un tipo puede tener MUCHAS       │
│ (implementar Arbitrary una    │ estrategias diferentes            │
│ sola vez)                     │ any::<i32>(), 0..100, Just(5)    │
│                               │                                  │
│ Shrinking acoplado al tipo:   │ Shrinking acoplado a la estrategia:│
│ i32 siempre shrinkea igual    │ 0..100 shrinkea diferente que     │
│                               │ 50..100 (distinta estructura)    │
│                               │                                  │
│ No puedes restringir rangos   │ Puedes restringir fácilmente:    │
│ fácilmente sin nuevo tipo     │ 1..100i32, "[a-z]{3,10}"         │
│                               │                                  │
│ Simple para tipos simples     │ Más potente para tipos complejos │
│                               │                                  │
│ Implementar Arbitrary:        │ Crear estrategia:                │
│ impl Arbitrary for MiTipo     │ fn mi_strategy() -> impl Strategy│
│                               │ o prop_compose!                  │
└───────────────────────────────┴──────────────────────────────────┘
```

---

## 6. Generadores built-in: cobertura de tipos

### Tipos con soporte nativo

```
┌──────────────────┬────────────────┬────────────────────────────┐
│ Tipo             │ quickcheck     │ proptest                   │
├──────────────────┼────────────────┼────────────────────────────┤
│ bool             │ ✅ Arbitrary   │ ✅ any::<bool>()           │
│ i8..i128         │ ✅ Arbitrary   │ ✅ any::<i32>() o rangos   │
│ u8..u128         │ ✅ Arbitrary   │ ✅ any::<u64>() o rangos   │
│ f32, f64         │ ✅ Arbitrary   │ ✅ any::<f64>() + subsets  │
│                  │ (puede generar │ (NORMAL, POSITIVE, etc.)   │
│                  │  NaN e Inf)    │                            │
│ char             │ ✅ Arbitrary   │ ✅ any::<char>() + rangos  │
│ String           │ ✅ Arbitrary   │ ✅ any::<String>() + REGEX │
│ Vec<T>           │ ✅ si T:Arb    │ ✅ any + collection::vec   │
│ Option<T>        │ ✅ si T:Arb    │ ✅ any::<Option<T>>()      │
│ Result<T,E>      │ ✅ si T,E:Arb  │ ✅ any::<Result<T,E>>()   │
│ HashMap<K,V>     │ ✅ si K,V:Arb  │ ✅ collection::hash_map   │
│ HashSet<T>       │ ✅ si T:Arb    │ ✅ collection::hash_set   │
│ BTreeMap<K,V>    │ ✅ si K,V:Arb  │ ✅ collection::btree_map  │
│ BTreeSet<T>      │ ✅ si T:Arb    │ ✅ collection::btree_set  │
│ Box<T>           │ ✅ si T:Arb    │ ✅ any::<T>().prop_map(Box)│
│ Rc<T>, Arc<T>    │ ✅ si T:Arb    │ ✅ similar a Box           │
│ (A, B) tuplas    │ ✅ hasta 8     │ ✅ hasta 12                │
│ [T; N] arrays    │ ✅ hasta 32    │ ✅ any::<[T; N]>()         │
│ Rangos (0..100)  │ ❌ no directo  │ ✅ nativo                  │
│ Regex ("[a-z]+") │ ❌ no          │ ✅ nativo                  │
│ VecDeque<T>      │ ✅ si T:Arb    │ ✅ collection::vec_deque   │
│ LinkedList<T>    │ ✅ si T:Arb    │ ✅ collection::linked_list │
│ BinaryHeap<T>    │ ✅ si T:Arb    │ ✅ collection::binary_heap │
│ Duration         │ ✅ Arbitrary   │ ❌ manual                  │
│ PathBuf          │ ❌ no          │ ❌ no (pero regex ayuda)   │
│ IpAddr           │ ❌ no          │ ❌ no (pero regex ayuda)   │
└──────────────────┴────────────────┴────────────────────────────┘
```

### Diferencia clave: control de rangos

```rust
// ══════════════════════════════════════════════════════════════
// quickcheck: sin control nativo de rangos
// ══════════════════════════════════════════════════════════════

// Para generar un i32 en 0..100, necesitas:
// Opción A: filtrar con TestResult::discard
quickcheck! {
    fn prop_rango(x: i32) -> TestResult {
        if x < 0 || x >= 100 {
            return TestResult::discard();  // descarta ~99.999% de valores
        }
        TestResult::from_bool(process(x) > 0)
    }
}

// Opción B: crear un newtype wrapper
#[derive(Debug, Clone)]
struct SmallInt(i32);

impl Arbitrary for SmallInt {
    fn arbitrary(g: &mut Gen) -> SmallInt {
        SmallInt(i32::arbitrary(g) % 100)
    }
    fn shrink(&self) -> Box<dyn Iterator<Item = SmallInt>> {
        Box::new(self.0.shrink().map(SmallInt))
    }
}

quickcheck! {
    fn prop_rango_mejor(x: SmallInt) -> bool {
        process(x.0) > 0
    }
}
```

```rust
// ══════════════════════════════════════════════════════════════
// proptest: control nativo de rangos
// ══════════════════════════════════════════════════════════════

proptest! {
    #[test]
    fn prop_rango(x in 0..100i32) {
        prop_assert!(process(x) > 0);
        // ¡Eso es todo! Limpio, directo, eficiente
    }
}
```

---

## 7. Composición de generadores

### quickcheck: composición manual con Arbitrary

```rust
use quickcheck::{Arbitrary, Gen};

#[derive(Debug, Clone)]
struct Point {
    x: f64,
    y: f64,
}

impl Arbitrary for Point {
    fn arbitrary(g: &mut Gen) -> Point {
        Point {
            x: f64::arbitrary(g),
            y: f64::arbitrary(g),
        }
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = Point>> {
        // Shrinking manual: generar variantes reducidas
        let x = self.x;
        let y = self.y;
        Box::new(
            self.x.shrink().map(move |new_x| Point { x: new_x, y })
                .chain(self.y.shrink().map(move |new_y| Point { x, y: new_y }))
        )
    }
}

// Para tipos más complejos:
#[derive(Debug, Clone)]
struct Rectangle {
    top_left: Point,
    bottom_right: Point,
}

impl Arbitrary for Rectangle {
    fn arbitrary(g: &mut Gen) -> Rectangle {
        // Asegurar que top_left < bottom_right requiere lógica manual
        let p1 = Point::arbitrary(g);
        let p2 = Point::arbitrary(g);
        Rectangle {
            top_left: Point {
                x: p1.x.min(p2.x),
                y: p1.y.min(p2.y),
            },
            bottom_right: Point {
                x: p1.x.max(p2.x) + 0.001, // evitar width=0
                y: p1.y.max(p2.y) + 0.001,
            },
        }
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = Rectangle>> {
        // Shrinking manual complejo y propenso a errores
        Box::new(std::iter::empty()) // a menudo se omite
    }
}
```

### proptest: composición funcional con Strategy

```rust
use proptest::prelude::*;

// Composición con prop_compose!
prop_compose! {
    fn point_strategy()(
        x in proptest::num::f64::NORMAL,
        y in proptest::num::f64::NORMAL
    ) -> Point {
        Point { x, y }
    }
}

// Composición con dependencias (dos etapas)
prop_compose! {
    fn rectangle_strategy()(
        x1 in -1000.0f64..1000.0,
        y1 in -1000.0f64..1000.0,
        w in 0.001f64..500.0,
        h in 0.001f64..500.0
    ) -> Rectangle {
        Rectangle {
            top_left: Point { x: x1, y: y1 },
            bottom_right: Point { x: x1 + w, y: y1 + h },
        }
    }
}

// Composición con combinadores funcionales
fn colored_rectangle() -> impl Strategy<Value = (Rectangle, String)> {
    (
        rectangle_strategy(),
        prop_oneof!["red", "green", "blue", "yellow"],
    )
}

// Composición anidada: shrinking es automático y correcto
proptest! {
    #[test]
    fn test_rect(rect in rectangle_strategy()) {
        prop_assert!(rect.bottom_right.x > rect.top_left.x);
        prop_assert!(rect.bottom_right.y > rect.top_left.y);
    }
}
```

### Comparación de la ergonomía de composición

```
  quickcheck:                                proptest:
  ──────────────────────────────────         ──────────────────────────────
  
  1. Definir struct                          1. Definir struct
  2. impl Arbitrary {                        2. prop_compose! {
       fn arbitrary(g) → Self {                  fn strategy()(
         // construcción manual                    x in ..., y in ...
       }                                         ) -> Self { ... }
       fn shrink → Iterator {                 }
         // shrinking manual ← TEDIOSO        // Shrinking: AUTOMÁTICO ✓
       }                                     
     }                                       
                                             
  Líneas: ~20-40                             Líneas: ~5-10
  Shrinking: manual, propenso a bugs         Shrinking: automático, correcto
  Restricciones: lógica en arbitrary()       Restricciones: en las estrategias
  Composición: anidada manualmente           Composición: combinadores funcionales
```

---

## 8. Shrinking: la diferencia fundamental

El shrinking es donde proptest y quickcheck divergen de forma más significativa. Esta diferencia afecta la calidad de los reportes de error, la reproducibilidad y la facilidad de debugging.

### Modelo conceptual

```
  quickcheck: SHRINKING POR TIPO
  ═══════════════════════════════
  
  Valor que falla: vec![97, -42, 31, 15, -88]
  
  El TIPO Vec<i32> decide cómo shrinkear:
    1. Vec::shrink() genera candidatos:
       - Remover un elemento: [97, 31, 15, -88], [-42, 31, 15, -88], ...
       - Shrinkear un elemento: [0, -42, 31, 15, -88], ...
    2. Para cada candidato que falla:
       - Repetir recursivamente
    3. El proceso depende del ORDEN en que se generan candidatos
    4. Diferentes ejecuciones pueden producir diferentes shrinks
  
  
  proptest: SHRINKING POR VALOR (DETERMINÍSTICO)
  ═══════════════════════════════════════════════
  
  Valor que falla: vec![97, -42, 31, 15, -88]
  
  La ESTRATEGIA registra cómo se generó cada parte:
    vec_strategy generó longitud=5 con:
      elemento[0] generó 97 desde rango 0..100
      elemento[1] generó -42 desde rango -100..100
      ...
    
  Shrinking opera en la estructura de generación:
    1. Reducir longitud: 5 → 3 → 2 → 1
    2. Para cada longitud, reducir cada elemento hacia su "mínimo"
    3. El proceso es determinístico: misma semilla → mismo shrink
    4. Siempre reproduce el mismo resultado
```

### Diagrama de flujo comparado

```
  quickcheck shrinking:
  
  fallo: [97, -42, 31, 15, -88]
    │
    ├── shrink vec: remover elementos ──→ [-42, 31, 15, -88] falla?
    │                                        │
    │                                        ├── sí → shrink de nuevo
    │                                        │        [-42, 31] falla?
    │                                        │            │
    │                                        │            └── sí → [-42] falla?
    │                                        │                      │
    │                                        │                      └── sí → shrink -42
    │                                        │                              [-21] falla?
    │                                        │                               └── ...
    │                                        │
    │                                        └── no → intentar otro candidato
    │
    ├── shrink vec: remover otro ──→ [97, 31, 15, -88] falla?
    │   ...
    │
    └── shrink elementos: [0, -42, 31, 15, -88] falla?
        ...
    
  Resultado: depende del orden de exploración
  PUEDE dar diferentes resultados en diferentes corridas
  
  
  proptest shrinking:
  
  fallo: [97, -42, 31, 15, -88]
  representado como ValueTree con estructura:
    ┌─ len: BinarySearch(5, min=0)
    ├─ [0]: BinarySearch(97, min=0)
    ├─ [1]: BinarySearch(-42, min=-100)
    ├─ [2]: BinarySearch(31, min=0)
    ├─ [3]: BinarySearch(15, min=0)
    └─ [4]: BinarySearch(-88, min=-100)
  
  Paso 1: simplify len: 5→2, probar [97, -42]
    falla → aceptar, seguir reduciendo
  Paso 2: simplify len: 2→1, probar [97]
    pasa → rechazar, volver a 2
  Paso 3: simplify [0]: 97→48, probar [48, -42]
    pasa → rechazar, mantener 97 (depende del test)
  Paso 4: simplify [1]: -42→-21, probar [97, -21]
    falla → aceptar
  ...continúa hasta encontrar mínimo
  
  Resultado: SIEMPRE el mismo para la misma semilla
  100% DETERMINÍSTICO
```

---

## 9. Shrinking determinístico (proptest) en detalle

### Cómo proptest genera el "ValueTree"

```
  Cuando proptest genera un valor, no solo produce el valor final.
  Genera un ÁRBOL que registra cómo se construyó el valor
  y permite "desandar" la generación de forma controlada.
  
  Estrategia: vec(0..100i32, 3..8)
  
  Generación:
    1. Generar longitud: rng → 5 (de rango 3..8)
    2. Generar elem[0]: rng → 73 (de rango 0..100)
    3. Generar elem[1]: rng → 12 (de rango 0..100)
    4. Generar elem[2]: rng → 91 (de rango 0..100)
    5. Generar elem[3]: rng → 45 (de rango 0..100)
    6. Generar elem[4]: rng → 28 (de rango 0..100)
  
  ValueTree resultante:
    VecValueTree {
        len: BinarySearch { lo: 3, current: 5, hi: 8 }
        elements: [
            BinarySearch { lo: 0, current: 73, hi: 100 },
            BinarySearch { lo: 0, current: 12, hi: 100 },
            BinarySearch { lo: 0, current: 91, hi: 100 },
            BinarySearch { lo: 0, current: 45, hi: 100 },
            BinarySearch { lo: 0, current: 28, hi: 100 },
        ]
    }
```

### BinarySearch: el mecanismo de shrinking

```
  BinarySearch shrinking de 73 hacia 0:
  
  lo=0        current=73        hi=100
  ├─ simplify(): current = (lo + current) / 2 = 36
  │  ├─ si falla → aceptar: hi=73, current=36
  │  │  ├─ simplify(): current = (0+36)/2 = 18
  │  │  │  ├─ si falla → aceptar: current=18
  │  │  │  └─ si pasa → rechazar: lo=19, current=36
  │  │  └─ ...
  │  └─ si pasa → rechazar: lo=37, current=73
  │     ├─ simplify(): current = (37+73)/2 = 55
  │     └─ ...
  └─ ...
  
  Propiedades del BinarySearch:
  ✅ Siempre converge (lo y hi se acercan)
  ✅ Determinístico (mismos pasos para mismos valores)
  ✅ O(log n) pasos en el peor caso
  ✅ Encuentra el mínimo exacto
```

### Composición de shrinking en proptest

```
  Una tupla (i32, String, Vec<u8>) shrinkea así:
  
  1. Intentar simplify en el PRIMER componente (i32)
     Si falla → aceptar, repetir
     Si pasa → intentar siguiente componente
  
  2. Intentar simplify en el SEGUNDO componente (String)
     Si falla → aceptar, repetir
     Si pasa → intentar siguiente
  
  3. Intentar simplify en el TERCER componente (Vec<u8>)
     Si falla → aceptar, repetir
     Si pasa → no más simplificación posible
  
  4. Volver al primer componente e intentar de nuevo
     (puede haber nuevas simplificaciones posibles)
  
  El proceso termina cuando ningún componente puede simplificarse.
  
  IMPORTANTE: todo es determinístico y reproducible.
```

---

## 10. Shrinking basado en Arbitrary (quickcheck) en detalle

### Cómo quickcheck implementa shrink()

```rust
// Implementación real de shrink() para Vec<T> en quickcheck
impl<T: Arbitrary> Arbitrary for Vec<T> {
    fn arbitrary(g: &mut Gen) -> Vec<T> {
        let size = { let s = g.size(); usize::arbitrary(g) % s };
        (0..size).map(|_| T::arbitrary(g)).collect()
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = Vec<T>>> {
        // Estrategia: generar múltiples "versiones reducidas"
        
        // 1. Vectores más cortos (removiendo elementos)
        let removes = (0..self.len()).map(|i| {
            let mut v = self.clone();
            v.remove(i);
            v
        });
        
        // 2. Vectores con elementos shrinkeados
        let shrinks = (0..self.len()).flat_map(|i| {
            let v = self.clone();
            let elem = v[i].clone();
            elem.shrink().map(move |new_elem| {
                let mut v = v.clone();
                v[i] = new_elem;
                v
            })
        });
        
        Box::new(removes.chain(shrinks))
    }
}
```

### Problemas del modelo quickcheck

```
  PROBLEMA 1: Shrinking no determinístico
  ────────────────────────────────────────
  
  quickcheck usa Gen internamente, que depende de RNG.
  El orden de exploración de candidatos puede variar.
  
  Corrida 1: fallo mínimo encontrado = [-1]
  Corrida 2: fallo mínimo encontrado = [0, -1]
  Corrida 3: fallo mínimo encontrado = [-2]
  
  → Dificulta la reproducción de fallos
  
  
  PROBLEMA 2: Shrinking puede perder la restricción
  ──────────────────────────────────────────────────
  
  Si Arbitrary genera un valor con restricciones:
    arbitrary(g) → Rectangle con width > 0
  
  Pero shrink() puede violar esas restricciones:
    shrink() → Rectangle con width = 0 (¡inválido!)
  
  → Shrink invalida las precondiciones
  → El "caso mínimo" reportado puede ser un falso positivo
  
  
  PROBLEMA 3: Shrinking manual es tedioso y propenso a bugs
  ─────────────────────────────────────────────────────────
  
  Para un struct con 5 campos:
    fn shrink(&self) -> Box<dyn Iterator<Item = Self>> {
        // Necesitas shrinkear cada campo, combinar los iteradores,
        // respetar invariantes entre campos...
        // ~30-50 líneas de código repetitivo
    }
  
  → Muchos desarrolladores simplemente no implementan shrink()
  → Se pierde la mayor ventaja de PBT
```

### Ejemplo donde quickcheck falla en shrinking

```rust
// Struct con invariante: a < b
#[derive(Debug, Clone)]
struct Range {
    a: i32,
    b: i32,
}

impl Arbitrary for Range {
    fn arbitrary(g: &mut Gen) -> Range {
        let a = i32::arbitrary(g);
        let b = a + 1 + (u32::arbitrary(g) % 100) as i32;
        Range { a, b }
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = Range>> {
        // ❌ BUG SUTIL: shrinkear a y b independientemente
        // puede producir a >= b (viola la invariante)
        let a = self.a;
        let b = self.b;
        Box::new(
            self.a.shrink().map(move |new_a| Range { a: new_a, b })
                .chain(self.b.shrink().map(move |new_b| Range { a, b: new_b }))
        )
    }
}

// Con proptest, esto no es un problema:
prop_compose! {
    fn range_strategy()(
        a in any::<i32>()
    )(
        a in Just(a),
        gap in 1..100i32
    ) -> Range {
        Range { a, b: a.saturating_add(gap) }
    }
}
// El shrinking es automático y SIEMPRE mantiene a < b
// porque reduce `gap` hacia 1 (su mínimo), nunca hacia 0
```

---

## 11. Impacto práctico de las diferencias de shrinking

### Caso real: encontrar el caso mínimo que falla

```
  Propiedad: "mi parser nunca rechaza input válido"
  Input que falla: una string de 847 caracteres
  
  CON QUICKCHECK:
  ────────────────
  Shrinking intenta:
    847 chars → 423 chars → ??? → 200 chars → ???
    (el shrinking puede "perderse" en un subóptimo local)
    
  Resultado después de 65,536 pasos:
    "Caso mínimo": 47 caracteres
    (probablemente NO es el verdadero mínimo)
    
  Re-ejecutar:
    Segundo intento: 23 caracteres (¿diferente!)
    Tercer intento: 51 caracteres (¿¡otro diferente!?)
    
    
  CON PROPTEST:
  ─────────────
  Shrinking usa BinarySearch en la estructura:
    len=847 → len=423 → len=211 → ... → len=3
    Luego shrink cada carácter: 'z' → 'a'
    
  Resultado después de ~200 pasos:
    Caso mínimo: "a(" (2 caracteres)
    
  Re-ejecutar:
    Siempre: "a(" (determinístico)
    Guardado en .proptest-regressions (permanente)
```

### Tabla de impacto práctico

```
┌─────────────────────────────┬─────────────────┬───────────────────┐
│ Aspecto                     │ quickcheck      │ proptest          │
├─────────────────────────────┼─────────────────┼───────────────────┤
│ Caso mínimo encontrado      │ Subóptimo local │ Mínimo global     │
│                             │ (a menudo)      │ (casi siempre)    │
│                             │                 │                   │
│ Reproducibilidad del shrink │ No garantizada  │ 100% determinístico│
│                             │                 │                   │
│ Velocidad del shrinking     │ Rápida (pero    │ Ligeramente más   │
│                             │ menos precisa)  │ lenta pero precisa│
│                             │                 │                   │
│ Invariantes preservadas     │ Puede violarlas │ Siempre preserva  │
│ durante shrink              │                 │                   │
│                             │                 │                   │
│ Esfuerzo del desarrollador  │ Implementar     │ Automático        │
│ para shrink correcto        │ shrink() manual │ (gratis)          │
│                             │                 │                   │
│ Calidad del reporte de error│ Variable        │ Consistentemente  │
│                             │                 │ buena             │
│                             │                 │                   │
│ Regresión de fallos         │ No              │ Sí (.proptest-    │
│                             │ (no hay archivo)│  regressions)     │
└─────────────────────────────┴─────────────────┴───────────────────┘
```

---

## 12. Configuración y control del runner

### quickcheck: QuickCheck struct

```rust
use quickcheck::QuickCheck;

#[test]
fn test_con_config_qc() {
    // Configurar el número de tests y el tamaño
    QuickCheck::new()
        .tests(1000)        // número de iteraciones (default: 100)
        .max_tests(10000)   // máximo de intentos (incluye discards)
        .gen(quickcheck::Gen::new(200))  // "tamaño" del generador
        .quickcheck(my_property as fn(i32) -> bool);
}

fn my_property(x: i32) -> bool {
    x.wrapping_add(0) == x
}
```

### proptest: ProptestConfig

```rust
use proptest::prelude::*;

proptest! {
    #![proptest_config(ProptestConfig {
        cases: 1000,            // iteraciones (default: 256)
        max_shrink_iters: 5000, // máximo pasos de shrinking
        ..ProptestConfig::default()
    })]
    
    #[test]
    fn test_con_config_pt(x in any::<i32>()) {
        prop_assert_eq!(x.wrapping_add(0), x);
    }
}
```

### Comparación de opciones de configuración

```
┌──────────────────────┬─────────────────────┬──────────────────────┐
│ Configuración        │ quickcheck          │ proptest             │
├──────────────────────┼─────────────────────┼──────────────────────┤
│ Número de iteraciones│ .tests(n)           │ cases: n             │
│ Default              │ 100                 │ 256                  │
│                      │                     │                      │
│ Máximo intentos      │ .max_tests(n)       │ max_flat_map_regens  │
│                      │                     │                      │
│ Tamaño generación    │ Gen::new(size)      │ Controlado por cada  │
│                      │ (global)            │ estrategia (local)   │
│                      │                     │                      │
│ Semilla fija         │ No directo          │ TestRunner::          │
│                      │                     │   deterministic()    │
│                      │                     │                      │
│ Máx shrink pasos     │ No configurable     │ max_shrink_iters     │
│                      │ (hardcoded)         │                      │
│                      │                     │                      │
│ Variable de entorno  │ QUICKCHECK_TESTS    │ PROPTEST_CASES       │
│                      │ QUICKCHECK_MAX_TESTS│                      │
│                      │                     │                      │
│ Archivo de regresión │ No                  │ Automático (.proptest│
│                      │                     │ -regressions)        │
│                      │                     │                      │
│ Config por test      │ Usando QuickCheck:: │ #![proptest_config]  │
│                      │ new().quickcheck()  │ o TestRunner manual  │
└──────────────────────┴─────────────────────┴──────────────────────┘
```

---

## 13. Archivos de regresión

### quickcheck: no tiene soporte nativo

```
  quickcheck no guarda los fallos que encuentra.
  
  Si un test falla:
  1. Ves el error en la terminal
  2. Arreglas el bug
  3. Re-corres los tests
  4. ¿El test que encontró el bug vuelve a encontrarlo? DEPENDE.
     (porque usa semillas aleatorias diferentes cada vez)
  
  Consecuencia:
    - Puedes arreglar un bug y creer que está resuelto
    - Pero el test aleatorio puede no re-descubrirlo
    - El bug podría reaparecer sin que lo notes
    
  Workaround:
    Copiar el input que falló y escribir un test unitario manual.
    Funciona, pero pierde la automatización.
```

### proptest: archivos .proptest-regressions

```
  proptest guarda AUTOMÁTICAMENTE los inputs que causan fallo.
  
  Si un test falla:
  1. proptest reduce el input al mínimo
  2. Guarda la semilla en src/lib.rs.proptest-regressions
  3. Re-correr tests: proptest LEE el archivo y prueba
     la semilla guardada PRIMERO
  4. Si el bug sigue: fallo inmediato (detecta regresión)
  5. Si se arregló: pasa rápido y continúa con aleatorios
  
  Archivo .proptest-regressions:
  ┌────────────────────────────────────────────────────────┐
  │ # Seeds for failure cases proptest has found previously│
  │ # It is automatically generated                       │
  │ cc 9fabe39e23bd3e78f431a7d8b6f9e3a4d9e2c1b0a3f6e8d7  │
  │ cc a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4  │
  └────────────────────────────────────────────────────────┘
  
  Recomendación: commitear al repo para compartir con el equipo.
```

---

## 14. Generación de strings y regex

### quickcheck: strings completamente aleatorias

```rust
// quickcheck genera strings Unicode aleatorias
// No hay control sobre el contenido

quickcheck! {
    fn prop_string(s: String) -> bool {
        // s puede ser: "", "a", "🦀✨", "\u{0}", "ñ", etc.
        // No hay forma nativa de decir "solo ASCII" o "matchea /[a-z]+/"
        s.len() == s.len() // tautología, solo para ilustrar
    }
}

// Para strings controladas: crear newtypes
#[derive(Debug, Clone)]
struct AsciiString(String);

impl Arbitrary for AsciiString {
    fn arbitrary(g: &mut Gen) -> AsciiString {
        let size = usize::arbitrary(g) % g.size();
        let s: String = (0..size)
            .map(|_| {
                let byte = u8::arbitrary(g) % 95 + 32; // ASCII printable
                byte as char
            })
            .collect();
        AsciiString(s)
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = AsciiString>> {
        Box::new(self.0.shrink().map(AsciiString))
    }
}
```

### proptest: regex strategies nativas

```rust
// proptest puede generar strings desde un regex DIRECTAMENTE

proptest! {
    #[test]
    fn test_email(email in "[a-z]{3,10}@[a-z]{3,8}\\.(com|org|net)") {
        prop_assert!(email.contains('@'));
        prop_assert!(
            email.ends_with(".com") || 
            email.ends_with(".org") || 
            email.ends_with(".net")
        );
    }
    
    #[test]
    fn test_ipv4(ip in "[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}") {
        prop_assert_eq!(ip.matches('.').count(), 3);
    }
    
    #[test]
    fn test_uuid(
        uuid in "[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}"
    ) {
        prop_assert_eq!(uuid.len(), 36);
    }
    
    #[test]
    fn test_solo_ascii_lower(s in "[a-z]{1,20}") {
        prop_assert!(s.chars().all(|c| c.is_ascii_lowercase()));
    }
}
```

### Impacto de las regex strategies

```
  ┌─────────────────────────────────────────────────────────────┐
  │         IMPACTO DE REGEX STRATEGIES (solo proptest)         │
  ├─────────────────────────────────────────────────────────────┤
  │                                                             │
  │  SIN regex (quickcheck):                                    │
  │  ├── Generar emails: ~20 líneas de código manual            │
  │  ├── Generar IPs: ~15 líneas                                │
  │  ├── Generar fechas: ~25 líneas                             │
  │  ├── Generar UUIDs: ~30 líneas                              │
  │  └── Cada formato requiere un newtype + Arbitrary           │
  │                                                             │
  │  CON regex (proptest):                                      │
  │  ├── Generar emails: 1 línea "[a-z]+@[a-z]+\\.com"         │
  │  ├── Generar IPs: 1 línea "[0-9]{1,3}(\\.[0-9]{1,3}){3}"  │
  │  ├── Generar fechas: 1 línea                                │
  │  ├── Generar UUIDs: 1 línea                                 │
  │  └── El shrinking de regex-strings es automático            │
  │                                                             │
  │  Para testing de parsers, validadores y procesadores de     │
  │  texto, las regex strategies son una ventaja DECISIVA.      │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

---

## 15. Estructuras recursivas

### quickcheck: recursión con size limit manual

```rust
use quickcheck::{Arbitrary, Gen};

#[derive(Debug, Clone)]
enum Tree {
    Leaf(i32),
    Node(Box<Tree>, i32, Box<Tree>),
}

impl Arbitrary for Tree {
    fn arbitrary(g: &mut Gen) -> Tree {
        let size = g.size();
        if size == 0 {
            // Caso base cuando el tamaño se agota
            Tree::Leaf(i32::arbitrary(g))
        } else {
            // Reducir el tamaño para las llamadas recursivas
            let half = Gen::new(size / 2);
            if bool::arbitrary(g) {
                Tree::Leaf(i32::arbitrary(g))
            } else {
                // ⚠️ PROBLEMA: Gen no tiene un método para reducir tamaño
                // Hay que crear un nuevo Gen, lo cual NO es straightforward
                // La documentación de quickcheck es poco clara sobre esto
                Tree::Node(
                    Box::new(Tree::Leaf(i32::arbitrary(g))), // simplificado
                    i32::arbitrary(g),
                    Box::new(Tree::Leaf(i32::arbitrary(g))), // simplificado
                )
            }
        }
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = Tree>> {
        // Shrinking de árboles es MUY complejo manualmente
        // La mayoría de la gente no lo implementa
        match self {
            Tree::Leaf(v) => Box::new(v.shrink().map(Tree::Leaf)),
            Tree::Node(l, v, r) => {
                // Intentar reemplazar todo el Node con un Leaf
                let v = *v;
                let leafs = v.shrink().map(Tree::Leaf);
                
                // Intentar shrinkear subárboles
                let left = l.clone();
                let right = r.clone();
                let shrink_left = left.shrink().map(move |new_l| {
                    Tree::Node(Box::new(new_l), v, Box::new(right.clone()))
                });
                // ... cada vez más complejo
                
                Box::new(leafs.chain(shrink_left))
            }
        }
    }
}
```

### proptest: prop_recursive elegante

```rust
use proptest::prelude::*;

#[derive(Debug, Clone)]
enum Tree {
    Leaf(i32),
    Node(Box<Tree>, i32, Box<Tree>),
}

fn tree_strategy() -> impl Strategy<Value = Tree> {
    // Caso base
    let leaf = any::<i32>().prop_map(Tree::Leaf);
    
    // Expansión recursiva con control de profundidad
    leaf.prop_recursive(
        4,     // profundidad máxima
        64,    // máximo de nodos
        2,     // items por nivel de recursión
        |inner| {
            // inner es la estrategia para subárboles
            (inner.clone(), any::<i32>(), inner)
                .prop_map(|(left, value, right)| {
                    Tree::Node(Box::new(left), value, Box::new(right))
                })
        },
    )
}

proptest! {
    #[test]
    fn test_tree(tree in tree_strategy()) {
        fn depth(t: &Tree) -> usize {
            match t {
                Tree::Leaf(_) => 1,
                Tree::Node(l, _, r) => 1 + depth(l).max(depth(r)),
            }
        }
        prop_assert!(depth(&tree) <= 5);
        // Shrinking es AUTOMÁTICO y correcto
    }
}
```

### Comparación para estructuras recursivas

```
┌──────────────────────┬────────────────────────┬───────────────────────┐
│ Aspecto              │ quickcheck             │ proptest              │
├──────────────────────┼────────────────────────┼───────────────────────┤
│ Definir generador    │ Manejo manual del      │ prop_recursive con    │
│ recursivo            │ tamaño en Gen          │ parámetros claros     │
│                      │                        │                       │
│ Control de           │ g.size() + lógica      │ depth, max_nodes,     │
│ profundidad          │ manual                 │ items_per_level       │
│                      │                        │                       │
│ Shrinking            │ Manual, muy tedioso    │ Automático, correcto  │
│                      │ (a menudo se omite)    │                       │
│                      │                        │                       │
│ Líneas de código     │ ~30-80                 │ ~10-15                │
│                      │                        │                       │
│ Riesgo de            │ Alto (stack overflow   │ Bajo (límites         │
│ recursión infinita   │ si Gen.size mal usado) │ explícitos)           │
│                      │                        │                       │
│ Ejemplo: JSON AST    │ ~80 líneas             │ ~20 líneas            │
│ generador + shrink   │                        │                       │
└──────────────────────┴────────────────────────┴───────────────────────┘
```

---

## 16. Testing de máquinas de estado

### quickcheck: secuencias de operaciones

```rust
use quickcheck::{Arbitrary, Gen};

#[derive(Debug, Clone)]
enum Op {
    Push(i32),
    Pop,
    Peek,
}

impl Arbitrary for Op {
    fn arbitrary(g: &mut Gen) -> Op {
        match u8::arbitrary(g) % 3 {
            0 => Op::Push(i32::arbitrary(g)),
            1 => Op::Pop,
            _ => Op::Peek,
        }
    }
    
    fn shrink(&self) -> Box<dyn Iterator<Item = Op>> {
        match self {
            Op::Push(v) => Box::new(v.shrink().map(Op::Push)),
            _ => Box::new(std::iter::empty()),
        }
    }
}

quickcheck! {
    fn prop_stack_consistent(ops: Vec<Op>) -> bool {
        let mut stack = Vec::new();
        let mut model = Vec::new();
        
        for op in ops {
            match op {
                Op::Push(v) => {
                    stack.push(v);
                    model.push(v);
                }
                Op::Pop => {
                    let a = stack.pop();
                    let b = model.pop();
                    if a != b { return false; }
                }
                Op::Peek => {
                    let a = stack.last();
                    let b = model.last();
                    if a != b { return false; }
                }
            }
        }
        true
    }
}
```

### proptest: misma idea, mejor ergonomía

```rust
use proptest::prelude::*;

#[derive(Debug, Clone)]
enum Op {
    Push(i32),
    Pop,
    Peek,
}

fn op_strategy() -> impl Strategy<Value = Op> {
    prop_oneof![
        any::<i32>().prop_map(Op::Push),
        Just(Op::Pop),
        Just(Op::Peek),
    ]
}

proptest! {
    #[test]
    fn prop_stack_consistent(
        ops in proptest::collection::vec(op_strategy(), 0..100)
    ) {
        let mut stack = Vec::new();
        let mut model = Vec::new();
        
        for op in &ops {
            match op {
                Op::Push(v) => {
                    stack.push(*v);
                    model.push(*v);
                }
                Op::Pop => {
                    let a = stack.pop();
                    let b = model.pop();
                    prop_assert_eq!(a, b);
                }
                Op::Peek => {
                    prop_assert_eq!(stack.last(), model.last());
                }
            }
        }
    }
}
```

### Diferencia clave en máquinas de estado

```
  quickcheck: cuando la secuencia de ops es larga y falla,
  el shrinking intenta acortar la secuencia y reducir los
  valores. PERO puede producir secuencias que tienen
  operaciones shrinkeadas de forma inconsistente.
  
  proptest: el shrinking reduce la secuencia de forma
  controlada — primero reduce la longitud, luego simplifica
  las operaciones individuales. El caso mínimo es más útil
  para debugging.
  
  Ejemplo de caso mínimo:
  
  quickcheck podría reportar:
    ops = [Push(42), Pop, Push(17), Push(-3), Peek, Pop, Pop]
    (7 operaciones — subóptimo)
    
  proptest reportaría:
    ops = [Push(0), Pop, Pop]
    (3 operaciones — mínimo: el bug es que Pop en stack vacío falla)
```

---

## 17. Integración con el ecosistema

### Crates que usan quickcheck

```
  quickcheck se integra bien con:
  ├── num         — propiedades numéricas
  ├── regex       — testing del motor de regex
  ├── serde       — (algunos tests internos)
  └── ripgrep     — el propio proyecto de BurntSushi

  Muchos crates legacy dependen de quickcheck
  para sus tests internos.
```

### Crates que usan proptest

```
  proptest se integra bien con:
  ├── proptest-derive  — derive Arbitrary automáticamente
  ├── proptest-state-machine — testing de máquinas de estado
  ├── test-strategy    — macros alternativas para proptest
  ├── bolero           — unifica fuzzing con proptest como backend
  ├── arbitrary        — trait Arbitrary compatible con fuzzers
  └── tokio/async      — proptest funciona con tests async

  proptest es el estándar de facto para nuevos proyectos.
```

### proptest-derive: derivar estrategias automáticamente

```rust
// Con proptest-derive, puedes derivar Arbitrary

// Cargo.toml
// [dev-dependencies]
// proptest = "1"
// proptest-derive = "0.4"

use proptest::prelude::*;
use proptest_derive::Arbitrary;

#[derive(Debug, Arbitrary)]
enum Color {
    Red,
    Green,
    Blue,
    Custom(u8, u8, u8),
}

#[derive(Debug, Arbitrary)]
struct Config {
    name: String,
    count: u32,
    enabled: bool,
    color: Color,
}

// Ahora any::<Config>() funciona automáticamente
proptest! {
    #[test]
    fn test_config(config in any::<Config>()) {
        // proptest genera Config con valores aleatorios
        // incluyendo todas las variantes de Color
        let _ = config;
    }
}
```

> quickcheck no tiene un equivalente tan pulido de `proptest-derive`.

### test-strategy: macros alternativas

```rust
// test-strategy ofrece una sintaxis alternativa para proptest
// que puede resultar más limpia para algunos patrones

// Cargo.toml:
// [dev-dependencies]
// proptest = "1"
// test-strategy = "0.3"

use test_strategy::proptest;

#[proptest]
fn test_simple(#[strategy(0..100i32)] x: i32) {
    assert!(x >= 0 && x < 100);
}

// Comparar con proptest nativo:
// proptest! {
//     #[test]
//     fn test_simple(x in 0..100i32) { ... }
// }
```

---

## 18. Rendimiento: velocidad de generación y shrinking

### Benchmarks comparativos

```
  Test: generar y verificar 10,000 Vec<i32> (0-100 elementos)
  
  ┌───────────────────┬─────────────┬──────────────┐
  │ Métrica           │ quickcheck  │ proptest     │
  ├───────────────────┼─────────────┼──────────────┤
  │ Generación (10K)  │ ~120ms      │ ~150ms       │
  │ Generación (1K)   │ ~12ms       │ ~15ms        │
  │ Overhead por test │ Mínimo      │ ~25% mayor   │
  │ Compilación       │ ~3s         │ ~8s          │
  │                   │             │              │
  │ Shrinking         │ Rápido pero │ Más lento    │
  │ (encontrar caso   │ impreciso   │ pero preciso │
  │ mínimo)           │             │              │
  └───────────────────┴─────────────┴──────────────┘
  
  Conclusión:
  ├── quickcheck es ~20% más rápido en generación pura
  ├── proptest compila más lento (más dependencias)
  ├── proptest produce mejores shrinks (compensa el overhead)
  └── Para la mayoría de proyectos, la diferencia es despreciable
```

### Cuándo el rendimiento importa

```
  El rendimiento de generación importa cuando:
  ├── Corres >100,000 iteraciones por test
  ├── Cada iteración es trivial (la generación domina)
  └── Tienes miles de property tests en el CI
  
  En la práctica:
  ├── El tiempo de generación es < 1% del tiempo total
  │   del test suite para la mayoría de proyectos
  ├── El time-to-fix importa más que time-to-run
  │   (mejor shrinking → bug se entiende más rápido)
  └── La compilación se amortiza en CI con caching
```

---

## 19. Ergonomía y curva de aprendizaje

### Curva de aprendizaje

```
  COMPLEJIDAD DE APRENDIZAJE
  
  Eje Y: dificultad
  Eje X: complejidad de la tarea
  
  dificultad
      │
   10 │                                      ╱ quickcheck
      │                                    ╱   (crece rápido
      │                                  ╱      para composición)
    8 │                                ╱
      │                              ╱
    6 │                         ╱──╱
      │                       ╱
    4 │                  ╱──╱        ╱─────── proptest
      │                ╱           ╱          (crece lento)
    2 │    ╱──────────╱          ╱
      │  ╱─────────╱           ╱
    0 │╱─────────╱────────────╱───────────────────────
      └────────────────────────────────────────────── complejidad
       simple    rangos   struct   recursivo  estado
       bool/int  custom   custom   JSON AST   máquina
  
  quickcheck: más simple al inicio, se complica rápido
  proptest:   ligeramente más complejo al inicio, escala mejor
```

### Errores típicos de principiantes

```
  En quickcheck:
  ├── Olvidar implementar shrink() → casos mínimos inútiles
  ├── Gen::new(size) sin entender qué es "size"
  ├── Shrink que viola invariantes del tipo
  ├── TestResult::discard() excesivo (>50% descartado)
  └── No saber cómo generar valores con restricciones
  
  En proptest:
  ├── Usar assert! en lugar de prop_assert!
  ├── prop_compose! con segunda etapa sin Just()
  ├── Filtros demasiado agresivos (too many rejections)
  ├── Confundir prop_map con prop_flat_map
  └── No commitear archivos .proptest-regressions
```

---

## 20. Tabla comparativa exhaustiva

```
┌───────────────────────────┬──────────────────────┬───────────────────────┐
│ Característica            │ quickcheck           │ proptest              │
├───────────────────────────┼──────────────────────┼───────────────────────┤
│ Versión actual            │ 1.x (estable)        │ 1.x (activa)          │
│ Inspiración               │ QuickCheck (Haskell)  │ Hypothesis (Python)   │
│ Descargas mensuales       │ ~400K                │ ~600K                 │
│ Mantenimiento             │ Bajo (maduro)        │ Activo                │
│                           │                      │                       │
│ GENERACIÓN                │                      │                       │
│ Trait principal            │ Arbitrary            │ Strategy              │
│ Generación + shrinking    │ Acoplados en tipo     │ Desacoplados          │
│ Rangos nativos            │ No                   │ Sí (0..100i32)        │
│ Regex strategies          │ No                   │ Sí ("[a-z]+")         │
│ Composición funcional     │ Limitada             │ Rica (map, filter,     │
│                           │                      │ flat_map, oneof, etc.) │
│ Derive automático         │ No oficial           │ proptest-derive       │
│ Tipos recursivos          │ Manual (g.size())    │ prop_recursive        │
│ Tuplas como estrategia    │ Hasta 8              │ Hasta 12              │
│ Control de tamaño         │ Gen::new(size) global│ Por estrategia (local)│
│                           │                      │                       │
│ SHRINKING                 │                      │                       │
│ Tipo de shrinking         │ Por tipo (Arbitrary) │ Por valor (ValueTree) │
│ Determinístico            │ No                   │ Sí                    │
│ Preserva invariantes      │ No garantizado       │ Sí (por construcción) │
│ Esfuerzo del desarrollador│ Manual (impl shrink) │ Automático            │
│ Calidad del caso mínimo   │ Variable             │ Consistentemente buena│
│                           │                      │                       │
│ CONFIGURACIÓN             │                      │                       │
│ Iteraciones default       │ 100                  │ 256                   │
│ Config por test           │ QuickCheck::new()    │ ProptestConfig        │
│ Variable de entorno       │ QUICKCHECK_TESTS     │ PROPTEST_CASES        │
│ Semilla fija              │ No directo           │ TestRunner::           │
│                           │                      │   deterministic()     │
│                           │                      │                       │
│ REGRESIÓN                 │                      │                       │
│ Archivos de regresión     │ No                   │ Sí (.proptest-        │
│                           │                      │   regressions)        │
│ Reproducibilidad          │ Baja                 │ Alta                  │
│                           │                      │                       │
│ MACROS                    │                      │                       │
│ Macro principal           │ quickcheck! {}       │ proptest! {}          │
│ Macro de atributo         │ #[quickcheck]        │ No (hay test-strategy)│
│ Composición               │ No                   │ prop_compose! {}      │
│ Union                     │ No                   │ prop_oneof! []        │
│ Assert                    │ return bool          │ prop_assert! ()       │
│ Filtrar                   │ TestResult::discard  │ prop_assume! ()       │
│                           │                      │                       │
│ RENDIMIENTO               │                      │                       │
│ Velocidad generación      │ Ligeramente más      │ Ligeramente más lento │
│                           │ rápido (~20%)        │                       │
│ Tiempo compilación        │ ~3s (menos deps)     │ ~8s (más deps)        │
│ Overhead runtime          │ Bajo                 │ Bajo-medio            │
│                           │                      │                       │
│ ECOSISTEMA                │                      │                       │
│ Derive crate              │ No oficial           │ proptest-derive       │
│ State machines            │ Manual               │ proptest-state-machine│
│ Fuzzing bridge            │ No                   │ bolero (backend)      │
│ Async support             │ Limitado             │ Sí                    │
│ Documentación             │ Básica               │ Extensa               │
│                           │                      │                       │
│ RECOMENDACIÓN             │ Proyectos legacy,    │ Nuevos proyectos,     │
│                           │ tests simples        │ tests complejos       │
└───────────────────────────┴──────────────────────┴───────────────────────┘
```

---

## 21. Cuándo usar cada uno

### Usar quickcheck cuando

```
  ✅ QUICKCHECK ES BUENA OPCIÓN SI:
  
  ├── Ya lo usas: migrar no aporta valor suficiente
  │   (si tu suite de tests funciona bien, no rompas lo que funciona)
  │
  ├── Tests ultra simples: bool, i32 → bool
  │   (quickcheck brilla en propiedades de una línea)
  │
  ├── Tiempo de compilación es crítico
  │   (quickcheck compila más rápido por tener menos dependencias)
  │
  ├── Dependencia mínima: no quieres arrastrar regex-syntax
  │   (proptest trae ~15 dependencias transitivas)
  │
  ├── Shrinking no es importante para tu caso
  │   (tests donde el input ya es simple y no necesita reducción)
  │
  └── Tu equipo ya conoce QuickCheck de Haskell/Erlang
      (la API de quickcheck es un port directo)
```

### Usar proptest cuando

```
  ✅ PROPTEST ES MEJOR OPCIÓN SI:
  
  ├── Proyecto nuevo: sin deuda técnica, elegir lo mejor
  │
  ├── Necesitas rangos: 0..100i32, 'a'..'z'
  │   (en quickcheck necesitas newtypes, en proptest es nativo)
  │
  ├── Necesitas regex: "[a-z]+@[a-z]+\\.com"
  │   (quickcheck no tiene esto, punto)
  │
  ├── Tipos complejos: structs con invariantes entre campos
  │   (prop_compose! con dos etapas, shrinking automático)
  │
  ├── Estructuras recursivas: árboles, JSON, ASTs
  │   (prop_recursive es mucho más ergonómico)
  │
  ├── Shrinking importa: debugging de fallos complejos
  │   (el shrinking determinístico de proptest es superior)
  │
  ├── Reproducibilidad: CI debe reportar los mismos fallos
  │   (archivos .proptest-regressions)
  │
  ├── Testing de máquinas de estado
  │   (proptest-state-machine crate)
  │
  └── Bridge con fuzzing
      (bolero usa proptest como backend)
```

### Diagrama de decisión

```
  ¿Necesitas regex strategies?
    │
    ├── SÍ → PROPTEST
    │
    └── NO → ¿Tipos complejos con invariantes?
                │
                ├── SÍ → PROPTEST (shrinking automático)
                │
                └── NO → ¿Proyecto nuevo?
                          │
                          ├── SÍ → PROPTEST (estándar de facto)
                          │
                          └── NO → ¿Ya usas quickcheck y funciona?
                                    │
                                    ├── SÍ → QUICKCHECK (no migrar sin razón)
                                    │
                                    └── NO → PROPTEST
```

---

## 22. Migrar de quickcheck a proptest

### Guía de traducción

```
┌──────────────────────────────────────────────────────────────────┐
│              GUÍA DE MIGRACIÓN quickcheck → proptest             │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. CARGO.TOML                                                   │
│     - quickcheck = "1"          →  proptest = "1"                │
│     - quickcheck_macros = "1"   →  (no necesario)                │
│                                                                  │
│  2. IMPORTS                                                      │
│     - use quickcheck::*         →  use proptest::prelude::*      │
│     - use quickcheck_macros::*  →  (no necesario)                │
│                                                                  │
│  3. MACRO PRINCIPAL                                              │
│     - quickcheck! {             →  proptest! {                   │
│         fn name(x: T) -> bool       #[test]                     │
│       }                              fn name(x in any::<T>()) {  │
│                                        prop_assert!(...);        │
│                                      }                           │
│                                    }                             │
│                                                                  │
│  4. ATRIBUTO                                                     │
│     - #[quickcheck]             →  proptest! { #[test] fn ... }  │
│       fn name(x: T) -> bool         (dentro del bloque)         │
│                                                                  │
│  5. FILTRADO                                                     │
│     - TestResult::discard()     →  prop_assume!(condición)       │
│                                                                  │
│  6. RESULTADO                                                    │
│     - return false              →  prop_assert!(false)           │
│     - return true               →  (implícito)                   │
│                                                                  │
│  7. ARBITRARY PARA TIPOS PROPIOS                                 │
│     - impl Arbitrary for T {    →  prop_compose! {               │
│         fn arbitrary(g) → T         fn t_strategy()(             │
│         fn shrink() → Iter             x in ..., y in ...        │
│       }                              ) -> T { ... }              │
│                                    }                             │
│                                    // O: impl Arbitrary          │
│                                    // con proptest-derive        │
│                                                                  │
│  8. RANGOS                                                       │
│     - SmallInt(i32 % 100)       →  0..100i32                     │
│       (newtype wrapper)              (directo)                   │
│                                                                  │
│  9. STRINGS CON FORMATO                                          │
│     - AsciiString (newtype)     →  "[a-z]+" (regex directo)      │
│                                                                  │
│  10. CONFIGURACIÓN                                               │
│      - QuickCheck::new()        →  ProptestConfig { ... }        │
│        .tests(n)                    cases: n                     │
│        .quickcheck(f)               #![proptest_config(...)]     │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Ejemplo de migración paso a paso

```rust
// ══════════════════════════════════════════════════════════════
// ANTES: quickcheck
// ══════════════════════════════════════════════════════════════

use quickcheck::{quickcheck, Arbitrary, Gen, TestResult};

#[derive(Debug, Clone)]
struct NonEmptyVec(Vec<i32>);

impl Arbitrary for NonEmptyVec {
    fn arbitrary(g: &mut Gen) -> NonEmptyVec {
        let mut v = Vec::<i32>::arbitrary(g);
        if v.is_empty() {
            v.push(i32::arbitrary(g));
        }
        NonEmptyVec(v)
    }
    fn shrink(&self) -> Box<dyn Iterator<Item = NonEmptyVec>> {
        Box::new(
            self.0.shrink()
                .filter(|v| !v.is_empty())
                .map(NonEmptyVec)
        )
    }
}

quickcheck! {
    fn prop_sort_preserves_length(v: NonEmptyVec) -> bool {
        let mut sorted = v.0.clone();
        sorted.sort();
        sorted.len() == v.0.len()
    }
    
    fn prop_division(a: i32, b: i32) -> TestResult {
        if b == 0 || (a == i32::MIN && b == -1) {
            return TestResult::discard();
        }
        TestResult::from_bool(a / b * b + a % b == a)
    }
}
```

```rust
// ══════════════════════════════════════════════════════════════
// DESPUÉS: proptest
// ══════════════════════════════════════════════════════════════

use proptest::prelude::*;

proptest! {
    #[test]
    fn prop_sort_preserves_length(
        v in proptest::collection::vec(any::<i32>(), 1..100)
        //                                          ^^^ 1 garantiza no vacío
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        prop_assert_eq!(sorted.len(), v.len());
    }
    
    #[test]
    fn prop_division(a in any::<i32>(), b in any::<i32>()) {
        prop_assume!(b != 0);
        prop_assume!(!(a == i32::MIN && b == -1));
        prop_assert_eq!(a / b * b + a % b, a);
    }
}

// No más NonEmptyVec newtype: `1..100` en la estrategia basta
// No más shrink() manual: proptest lo hace automáticamente
// No más TestResult: prop_assume! es más limpio
// No más return bool: prop_assert! da mensajes informativos
```

---

## 23. Alternativas: bolero y otros

### bolero: unificando fuzzing y PBT

```
  ┌─────────────────────────────────────────────────────────────┐
  │                    BOLERO                                   │
  ├─────────────────────────────────────────────────────────────┤
  │                                                             │
  │  bolero unifica PBT y fuzzing bajo una sola API.            │
  │  Puede usar diferentes backends:                            │
  │                                                             │
  │  ├── proptest (PBT, rápido, sin instrumentación)            │
  │  ├── libfuzzer (fuzzing, encuentra crashes)                 │
  │  ├── afl (fuzzing, cobertura guiada)                        │
  │  └── kani (verificación formal)                             │
  │                                                             │
  │  La misma propiedad puede correr como:                      │
  │  - PBT rápido en desarrollo (cargo test)                    │
  │  - Fuzzing exhaustivo en CI (cargo bolero)                  │
  │                                                             │
  │  Cargo.toml:                                                │
  │    [dev-dependencies]                                       │
  │    bolero = "0.10"                                          │
  │                                                             │
  │  Uso:                                                       │
  │    use bolero::check;                                       │
  │                                                             │
  │    #[test]                                                  │
  │    fn test_parse() {                                        │
  │        check!()                                             │
  │            .with_type::<Vec<u8>>()                          │
  │            .for_each(|data| {                               │
  │                let _ = my_parser(data);                     │
  │            });                                              │
  │    }                                                        │
  │                                                             │
  │  Estado: nicho más especializado, menor adopción que        │
  │  proptest, pero prometedor para proyectos que necesitan     │
  │  AMBOS PBT y fuzzing.                                      │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

### arbitrary crate: trait compartido con fuzzers

```rust
// El crate `arbitrary` define un trait Arbitrary independiente
// que es compatible con libfuzzer, afl y cargo-fuzz.
// No es lo mismo que quickcheck::Arbitrary ni proptest::Arbitrary.

// Cargo.toml:
// [dependencies]
// arbitrary = { version = "1", features = ["derive"] }

use arbitrary::Arbitrary;

#[derive(Debug, Arbitrary)]
struct MyInput {
    name: String,
    value: u32,
    flag: bool,
}

// Este Arbitrary funciona con cargo-fuzz pero NO con proptest/quickcheck.
// bolero es el puente que unifica estos mundos.
```

### Resumen de alternativas

```
┌─────────────────┬──────────────────────────────────────────────┐
│ Crate           │ Rol                                          │
├─────────────────┼──────────────────────────────────────────────┤
│ proptest        │ PBT dominante, rico, maduro                  │
│ quickcheck      │ PBT clásico, simple, legado                  │
│ bolero          │ Unifica PBT + fuzzing, múltiples backends    │
│ arbitrary       │ Trait Arbitrary para fuzzers (no PBT)        │
│ proptest-derive │ Derive Arbitrary para proptest               │
│ test-strategy   │ Macros alternativas sobre proptest           │
│ proptest-state  │ Testing de máquinas de estado con proptest   │
│ -machine        │                                              │
└─────────────────┴──────────────────────────────────────────────┘
```

---

## 24. Comparación con PBT en C y Go

### C: theft

```c
// theft es el framework PBT más usado en C
// Filosofía: similar a quickcheck pero con API de C

// Cargo.toml equivalente: agregar theft como dependencia manual
// (no hay gestor de paquetes estándar en C)

// Definir tipo para generación
struct int_pair {
    int a;
    int b;
};

// Generador (allocate + fill)
static enum theft_alloc_res
pair_alloc(struct theft *t, void *env, void **output) {
    struct int_pair *p = malloc(sizeof(struct int_pair));
    if (!p) return THEFT_ALLOC_ERROR;
    
    p->a = (int)theft_random_bits(t, 32);
    p->b = (int)theft_random_bits(t, 32);
    *output = p;
    return THEFT_ALLOC_OK;
}

// Shrinking (manual, tedioso)
static enum theft_shrink_res
pair_shrink(struct theft *t, void *instance, uint32_t tactic, void **output) {
    struct int_pair *p = (struct int_pair *)instance;
    struct int_pair *new_p = malloc(sizeof(struct int_pair));
    if (!new_p) return THEFT_SHRINK_ERROR;
    
    switch (tactic) {
        case 0: // shrink a
            new_p->a = p->a / 2;
            new_p->b = p->b;
            break;
        case 1: // shrink b
            new_p->a = p->a;
            new_p->b = p->b / 2;
            break;
        default:
            free(new_p);
            return THEFT_SHRINK_NO_MORE_TACTICS;
    }
    *output = new_p;
    return THEFT_SHRINK_OK;
}

// Propiedad
static enum theft_trial_res
prop_add_commutative(struct theft *t, void *arg1) {
    struct int_pair *p = (struct int_pair *)arg1;
    return (p->a + p->b == p->b + p->a)
        ? THEFT_TRIAL_PASS
        : THEFT_TRIAL_FAIL;
}

// Ejecución
void test_commutative(void) {
    struct theft_type_info info = {
        .alloc = pair_alloc,
        .free = free,
        .shrink = pair_shrink,
    };
    
    struct theft_run_config config = {
        .prop1 = prop_add_commutative,
        .type_info = { &info },
        .trials = 1000,
    };
    
    struct theft *t = theft_init(0);
    theft_run(t, &config);
    theft_free(t);
}
```

### Go: testing/quick + gopter

```go
// ══════════════════════════════════════════════════════════════
// Go: testing/quick (built-in, simple pero limitado)
// ══════════════════════════════════════════════════════════════

package mypackage

import (
    "testing"
    "testing/quick"
)

func TestAddCommutative(t *testing.T) {
    f := func(a, b int32) bool {
        return a+b == b+a
    }
    if err := quick.Check(f, &quick.Config{MaxCount: 1000}); err != nil {
        t.Error(err)
    }
}

// Limitaciones de testing/quick:
// - Sin shrinking real
// - Sin control de rangos
// - Sin composición de generadores
// - Solo genera tipos que implementan Generate

// ══════════════════════════════════════════════════════════════
// Go: gopter (más potente, similar a proptest)
// ══════════════════════════════════════════════════════════════

import (
    "testing"
    "github.com/leanovate/gopter"
    "github.com/leanovate/gopter/gen"
    "github.com/leanovate/gopter/prop"
)

func TestWithGopter(t *testing.T) {
    parameters := gopter.DefaultTestParameters()
    parameters.MinSuccessfulTests = 1000
    
    properties := gopter.NewProperties(parameters)
    
    properties.Property("add is commutative", prop.ForAll(
        func(a, b int32) bool {
            return a+b == b+a
        },
        gen.Int32(),
        gen.Int32(),
    ))
    
    properties.TestingRun(t)
}
```

### Tabla comparativa completa con C y Go

```
┌──────────────────┬──────────────┬──────────────┬──────────────┬──────────────┐
│ Característica   │ C (theft)    │ Go (quick)   │ Rust (qc)    │ Rust (pt)    │
├──────────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ Ergonomía        │ Muy baja     │ Media        │ Media        │ Alta         │
│ Shrinking        │ Manual       │ No           │ Manual       │ Automático   │
│ Determinístico   │ No           │ N/A          │ No           │ Sí           │
│ Composición      │ No           │ No           │ Limitada     │ Rica         │
│ Regex            │ No           │ No           │ No           │ Sí           │
│ Regresión        │ No           │ No           │ No           │ Sí           │
│ Type safety      │ void*        │ interface{}  │ Genéricos    │ Genéricos    │
│ Líneas / test    │ ~30-50       │ ~10          │ ~5-10        │ ~5-10        │
│ Líneas / tipo    │ ~40-60       │ ~15-30       │ ~15-40       │ ~5-15        │
│                  │ custom       │              │              │              │
│ Built-in         │ No           │ Sí (quick)   │ No           │ No           │
│ Comunidad        │ Pequeña      │ Media        │ Media        │ Grande       │
│ Madurez          │ Estable      │ Estable      │ Estable      │ Activa       │
└──────────────────┴──────────────┴──────────────┴──────────────┴──────────────┘

Resumen:
  C:    PBT es posible pero dolorosamente verboso
  Go:   testing/quick es mínimo; gopter es mejor pero poco usado
  Rust: quickcheck es bueno; proptest es excelente
  
  Rust tiene el mejor ecosistema de PBT de los tres lenguajes.
```

---

## 25. Errores comunes al elegir framework

### Error 1: elegir quickcheck "porque es más popular"

```
  ❌ "quickcheck tiene más estrellas en GitHub"
  
  Realidad:
  - proptest tiene MÁS descargas mensuales (~600K vs ~400K)
  - Las estrellas de quickcheck son históricas (existe desde 2014)
  - La mayoría de proyectos nuevos eligen proptest
  - quickcheck está en modo mantenimiento, no innovación
```

### Error 2: elegir proptest cuando quickcheck basta

```
  ❌ "proptest es siempre mejor, así que lo uso para todo"
  
  Realidad:
  - Para tests triviales (fn(i32) -> bool), quickcheck es igual de bueno
  - proptest trae ~15 dependencias transitivas vs ~5 de quickcheck
  - Si tu proyecto NO necesita regex, composición o shrinking avanzado,
    quickcheck es una opción válida y más ligera
  - No migres lo que funciona sin una razón concreta
```

### Error 3: usar ambos en el mismo proyecto

```
  ❌ "Usamos quickcheck para algunos tests y proptest para otros"
  
  Problemas:
  - Dos traits Arbitrary diferentes (incompatibles)
  - Dos APIs de macros diferentes
  - Confusión para nuevos miembros del equipo
  - Duplicación de generadores para los mismos tipos
  
  ✅ Elegir UNO y usarlo consistentemente en todo el proyecto
```

### Error 4: no considerar bolero para fuzzing + PBT

```
  ❌ "Tenemos proptest para PBT y cargo-fuzz por separado"
  
  Oportunidad perdida:
  - bolero usa proptest como backend Y puede cambiar a libfuzzer
  - Misma propiedad, dos modos de ejecución
  - Menor duplicación de código
  
  Pero:
  - bolero es más nicho y menos documentado
  - Solo vale la pena si REALMENTE haces fuzzing
```

### Error 5: asumir que PBT reemplaza unit tests

```
  ❌ "Tengo proptest, no necesito tests unitarios"
  
  Realidad:
  - PBT complementa, no reemplaza unit tests
  - Casos edge conocidos deben ser tests explícitos
  - La documentación se beneficia de ejemplos concretos
  - PBT puede ser más lento que tests unitarios
  
  Mejor práctica:
    Unit tests: casos concretos importantes, documentación
    PBT: propiedades generales, descubrir bugs inesperados
```

---

## 26. Ejemplo completo: misma propiedad en ambos frameworks

### La propiedad: sort es correcto

Implementamos las MISMAS tres propiedades en ambos frameworks para comparar la experiencia.

### quickcheck: sort properties

```rust
// tests/qc_sort.rs

#[cfg(test)]
mod tests {
    use quickcheck::{quickcheck, TestResult};
    use std::collections::HashMap;
    
    // ── Propiedad 1: resultado está ordenado ─────────────────
    
    quickcheck! {
        fn prop_sorted_output(v: Vec<i32>) -> bool {
            let mut sorted = v;
            sorted.sort();
            sorted.windows(2).all(|w| w[0] <= w[1])
        }
    }
    
    // ── Propiedad 2: misma longitud ──────────────────────────
    
    quickcheck! {
        fn prop_preserves_length(v: Vec<i32>) -> bool {
            let original_len = v.len();
            let mut sorted = v;
            sorted.sort();
            sorted.len() == original_len
        }
    }
    
    // ── Propiedad 3: mismos elementos (permutación) ──────────
    
    quickcheck! {
        fn prop_preserves_elements(v: Vec<i32>) -> bool {
            let mut freq_orig: HashMap<i32, usize> = HashMap::new();
            for &x in &v {
                *freq_orig.entry(x).or_insert(0) += 1;
            }
            
            let mut sorted = v;
            sorted.sort();
            
            let mut freq_sorted: HashMap<i32, usize> = HashMap::new();
            for &x in &sorted {
                *freq_sorted.entry(x).or_insert(0) += 1;
            }
            
            freq_orig == freq_sorted
        }
    }
    
    // ── Propiedad 4: idempotente ─────────────────────────────
    
    quickcheck! {
        fn prop_idempotent(v: Vec<i32>) -> bool {
            let mut once = v.clone();
            once.sort();
            let mut twice = once.clone();
            twice.sort();
            once == twice
        }
    }
    
    // ── Propiedad 5: con rango (necesita TestResult) ─────────
    
    quickcheck! {
        fn prop_small_vec_sorted(v: Vec<i32>) -> TestResult {
            if v.len() > 20 {
                return TestResult::discard(); // solo vecs pequeñas
            }
            let mut sorted = v;
            sorted.sort();
            TestResult::from_bool(
                sorted.windows(2).all(|w| w[0] <= w[1])
            )
        }
    }
}
```

### proptest: las mismas propiedades

```rust
// tests/pt_sort.rs

use proptest::prelude::*;
use std::collections::HashMap;

proptest! {
    // ── Propiedad 1: resultado está ordenado ─────────────────
    
    #[test]
    fn prop_sorted_output(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let mut sorted = v;
        sorted.sort();
        for w in sorted.windows(2) {
            prop_assert!(w[0] <= w[1], "no ordenado: {} > {}", w[0], w[1]);
        }
    }
    
    // ── Propiedad 2: misma longitud ──────────────────────────
    
    #[test]
    fn prop_preserves_length(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let original_len = v.len();
        let mut sorted = v;
        sorted.sort();
        prop_assert_eq!(sorted.len(), original_len);
    }
    
    // ── Propiedad 3: mismos elementos (permutación) ──────────
    
    #[test]
    fn prop_preserves_elements(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let mut freq_orig: HashMap<i32, usize> = HashMap::new();
        for &x in &v {
            *freq_orig.entry(x).or_insert(0) += 1;
        }
        
        let mut sorted = v;
        sorted.sort();
        
        let mut freq_sorted: HashMap<i32, usize> = HashMap::new();
        for &x in &sorted {
            *freq_sorted.entry(x).or_insert(0) += 1;
        }
        
        prop_assert_eq!(freq_orig, freq_sorted);
    }
    
    // ── Propiedad 4: idempotente ─────────────────────────────
    
    #[test]
    fn prop_idempotent(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let mut once = v;
        once.sort();
        let mut twice = once.clone();
        twice.sort();
        prop_assert_eq!(once, twice);
    }
    
    // ── Propiedad 5: con rango (nativo, sin discard) ─────────
    
    #[test]
    fn prop_small_vec_sorted(
        v in proptest::collection::vec(any::<i32>(), 0..=20)
        //                                          ^^^^^ rango directo
    ) {
        let mut sorted = v;
        sorted.sort();
        for w in sorted.windows(2) {
            prop_assert!(w[0] <= w[1]);
        }
    }
}
```

### Comparación línea por línea

```
  Propiedad     │ quickcheck         │ proptest         │ Ventaja
  ──────────────┼────────────────────┼──────────────────┼────────────
  Setup         │ 2 líneas Cargo.toml│ 1 línea          │ proptest
  Import        │ 2 use              │ 1 use            │ proptest
  Tipo retorno  │ -> bool            │ (implícito)      │ proptest
  Error message │ "test failed"      │ formateado       │ proptest
  Rango de vec  │ TestResult::discard│ 0..=20 (nativo)  │ proptest
  Shrinking     │ OK (automático para│ Mejor (determi-  │ proptest
                │ Vec<i32>)          │ nístico)         │
  Simplicidad   │ Más conciso para   │ Más conciso para │ empate en
                │ tests triviales    │ tests con rangos │ simple
```

---

## 27. Programa de práctica

### Proyecto: implementar el MISMO test suite con ambos frameworks

Crea un proyecto que tenga los tests escritos en ambos frameworks para poder comparar la experiencia directamente.

### Estructura del proyecto

```
dual_pbt/
├── Cargo.toml
├── src/
│   └── lib.rs              # Código a testear
├── tests/
│   ├── with_quickcheck.rs   # Tests con quickcheck
│   └── with_proptest.rs     # Tests con proptest (equivalentes)
```

### Cargo.toml

```toml
[package]
name = "dual_pbt"
version = "0.1.0"
edition = "2021"

[dev-dependencies]
quickcheck = "1"
quickcheck_macros = "1"
proptest = "1"
```

### src/lib.rs — Código bajo test

```rust
use std::collections::HashMap;

/// Stack con capacidad limitada
#[derive(Debug, Clone)]
pub struct BoundedStack<T> {
    data: Vec<T>,
    capacity: usize,
}

#[derive(Debug, Clone, PartialEq)]
pub enum StackError {
    Overflow,
    Underflow,
}

impl<T: Clone + std::fmt::Debug> BoundedStack<T> {
    pub fn new(capacity: usize) -> Self {
        assert!(capacity > 0, "capacity must be > 0");
        BoundedStack {
            data: Vec::new(),
            capacity,
        }
    }

    pub fn push(&mut self, item: T) -> Result<(), StackError> {
        if self.data.len() >= self.capacity {
            return Err(StackError::Overflow);
        }
        self.data.push(item);
        Ok(())
    }

    pub fn pop(&mut self) -> Result<T, StackError> {
        self.data.pop().ok_or(StackError::Underflow)
    }

    pub fn peek(&self) -> Option<&T> {
        self.data.last()
    }

    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn is_full(&self) -> bool {
        self.data.len() >= self.capacity
    }

    pub fn capacity(&self) -> usize {
        self.capacity
    }
}

/// Tokenizador simple
pub fn tokenize(input: &str) -> Vec<String> {
    input
        .split_whitespace()
        .map(|s| s.to_lowercase())
        .filter(|s| !s.is_empty())
        .collect()
}

/// Contador de frecuencia de palabras
pub fn word_frequency(text: &str) -> HashMap<String, usize> {
    let mut freq = HashMap::new();
    for word in tokenize(text) {
        *freq.entry(word).or_insert(0) += 1;
    }
    freq
}

/// Compresión RLE simple
pub fn rle_encode(data: &[u8]) -> Vec<(u8, u8)> {
    if data.is_empty() {
        return Vec::new();
    }
    
    let mut result = Vec::new();
    let mut current = data[0];
    let mut count: u8 = 1;
    
    for &byte in &data[1..] {
        if byte == current && count < 255 {
            count += 1;
        } else {
            result.push((current, count));
            current = byte;
            count = 1;
        }
    }
    result.push((current, count));
    result
}

pub fn rle_decode(encoded: &[(u8, u8)]) -> Vec<u8> {
    let mut result = Vec::new();
    for &(byte, count) in encoded {
        for _ in 0..count {
            result.push(byte);
        }
    }
    result
}
```

### tests/with_quickcheck.rs

```rust
use dual_pbt::*;
use quickcheck::{quickcheck, Arbitrary, Gen, TestResult};
use std::collections::HashMap;

// ── Operaciones para BoundedStack ────────────────────────────

#[derive(Debug, Clone)]
enum StackOp {
    Push(i32),
    Pop,
    Peek,
}

impl Arbitrary for StackOp {
    fn arbitrary(g: &mut Gen) -> StackOp {
        match u8::arbitrary(g) % 3 {
            0 => StackOp::Push(i32::arbitrary(g)),
            1 => StackOp::Pop,
            _ => StackOp::Peek,
        }
    }
}

// ── BoundedStack: propiedades ────────────────────────────────

quickcheck! {
    fn qc_stack_len_bounded(ops: Vec<StackOp>) -> bool {
        let cap = 10;
        let mut stack = BoundedStack::new(cap);
        for op in ops {
            match op {
                StackOp::Push(v) => { let _ = stack.push(v); }
                StackOp::Pop => { let _ = stack.pop(); }
                StackOp::Peek => { let _ = stack.peek(); }
            }
            if stack.len() > cap {
                return false;
            }
        }
        true
    }
    
    fn qc_stack_push_pop_roundtrip(values: Vec<i32>) -> TestResult {
        if values.len() > 50 {
            return TestResult::discard();
        }
        let mut stack = BoundedStack::new(100);
        for &v in &values {
            stack.push(v).unwrap();
        }
        for &v in values.iter().rev() {
            if stack.pop().unwrap() != v {
                return TestResult::from_bool(false);
            }
        }
        TestResult::from_bool(stack.is_empty())
    }
    
    fn qc_stack_overflow(v: i32) -> bool {
        let mut stack = BoundedStack::new(1);
        stack.push(v).unwrap();
        stack.push(v) == Err(StackError::Overflow)
    }
    
    fn qc_stack_underflow() -> bool {
        let mut stack: BoundedStack<i32> = BoundedStack::new(1);
        stack.pop() == Err(StackError::Underflow)
    }
}

// ── Tokenize: propiedades ────────────────────────────────────

quickcheck! {
    fn qc_tokenize_lowercase(s: String) -> bool {
        tokenize(&s).iter().all(|t| t == &t.to_lowercase())
    }
    
    fn qc_tokenize_no_empty(s: String) -> bool {
        tokenize(&s).iter().all(|t| !t.is_empty())
    }
    
    fn qc_tokenize_no_whitespace(s: String) -> bool {
        tokenize(&s).iter().all(|t| !t.contains(char::is_whitespace))
    }
}

// ── RLE: propiedades ─────────────────────────────────────────

quickcheck! {
    fn qc_rle_roundtrip(data: Vec<u8>) -> TestResult {
        if data.len() > 500 {
            return TestResult::discard();
        }
        let encoded = rle_encode(&data);
        let decoded = rle_decode(&encoded);
        TestResult::from_bool(data == decoded)
    }
    
    fn qc_rle_empty() -> bool {
        rle_encode(&[]).is_empty() && rle_decode(&[]).is_empty()
    }
    
    fn qc_rle_counts_positive(data: Vec<u8>) -> TestResult {
        if data.is_empty() {
            return TestResult::discard();
        }
        let encoded = rle_encode(&data);
        TestResult::from_bool(
            encoded.iter().all(|&(_, count)| count > 0)
        )
    }
}

// ── Word frequency: propiedades ──────────────────────────────

quickcheck! {
    fn qc_freq_sum_equals_token_count(s: String) -> bool {
        let freq = word_frequency(&s);
        let sum: usize = freq.values().sum();
        let tokens = tokenize(&s);
        sum == tokens.len()
    }
    
    fn qc_freq_all_positive(s: String) -> bool {
        word_frequency(&s).values().all(|&v| v > 0)
    }
}
```

### tests/with_proptest.rs

```rust
use dual_pbt::*;
use proptest::prelude::*;
use std::collections::HashMap;

// ── Operaciones para BoundedStack ────────────────────────────

#[derive(Debug, Clone)]
enum StackOp {
    Push(i32),
    Pop,
    Peek,
}

fn stack_op_strategy() -> impl Strategy<Value = StackOp> {
    prop_oneof![
        any::<i32>().prop_map(StackOp::Push),
        Just(StackOp::Pop),
        Just(StackOp::Peek),
    ]
}

// ── BoundedStack: propiedades ────────────────────────────────

proptest! {
    #[test]
    fn pt_stack_len_bounded(
        ops in proptest::collection::vec(stack_op_strategy(), 0..100)
    ) {
        let cap = 10;
        let mut stack = BoundedStack::new(cap);
        for op in &ops {
            match op {
                StackOp::Push(v) => { let _ = stack.push(*v); }
                StackOp::Pop => { let _ = stack.pop(); }
                StackOp::Peek => { let _ = stack.peek(); }
            }
            prop_assert!(
                stack.len() <= cap,
                "len {} excede capacity {}", stack.len(), cap
            );
        }
    }
    
    #[test]
    fn pt_stack_push_pop_roundtrip(
        values in proptest::collection::vec(any::<i32>(), 0..50)
    ) {
        let mut stack = BoundedStack::new(100);
        for &v in &values {
            stack.push(v).unwrap();
        }
        for &v in values.iter().rev() {
            prop_assert_eq!(stack.pop().unwrap(), v);
        }
        prop_assert!(stack.is_empty());
    }
    
    #[test]
    fn pt_stack_overflow(v in any::<i32>()) {
        let mut stack = BoundedStack::new(1);
        stack.push(v).unwrap();
        prop_assert_eq!(stack.push(v), Err(StackError::Overflow));
    }
    
    #[test]
    fn pt_stack_underflow(_dummy in Just(())) {
        let mut stack: BoundedStack<i32> = BoundedStack::new(1);
        prop_assert_eq!(stack.pop(), Err(StackError::Underflow));
    }
}

// ── Tokenize: propiedades ────────────────────────────────────

proptest! {
    #[test]
    fn pt_tokenize_lowercase(s in any::<String>()) {
        for token in tokenize(&s) {
            prop_assert_eq!(&token, &token.to_lowercase());
        }
    }
    
    #[test]
    fn pt_tokenize_no_empty(s in any::<String>()) {
        for token in tokenize(&s) {
            prop_assert!(!token.is_empty());
        }
    }
    
    #[test]
    fn pt_tokenize_no_whitespace(s in any::<String>()) {
        for token in tokenize(&s) {
            prop_assert!(
                !token.contains(char::is_whitespace),
                "token '{}' contiene whitespace", token
            );
        }
    }
    
    // EXTRA: propiedades que son más fáciles con proptest
    
    #[test]
    fn pt_tokenize_idempotent(s in "[a-z ]{0,50}") {
        let tokens = tokenize(&s);
        for token in &tokens {
            let re_tokenized = tokenize(token);
            prop_assert_eq!(
                re_tokenized.len(), 1,
                "'{}' se re-tokenizó en {:?}", token, re_tokenized
            );
            prop_assert_eq!(&re_tokenized[0], token);
        }
    }
    
    #[test]
    fn pt_tokenize_ascii_input(s in "[a-zA-Z ]{0,100}") {
        let tokens = tokenize(&s);
        let total_chars: usize = tokens.iter().map(|t| t.len()).sum();
        let non_space_chars = s.chars().filter(|c| !c.is_whitespace()).count();
        prop_assert_eq!(total_chars, non_space_chars);
    }
}

// ── RLE: propiedades ─────────────────────────────────────────

proptest! {
    #[test]
    fn pt_rle_roundtrip(
        data in proptest::collection::vec(any::<u8>(), 0..500)
    ) {
        let encoded = rle_encode(&data);
        let decoded = rle_decode(&encoded);
        prop_assert_eq!(data, decoded);
    }
    
    #[test]
    fn pt_rle_empty(_dummy in Just(())) {
        prop_assert!(rle_encode(&[]).is_empty());
        prop_assert!(rle_decode(&[]).is_empty());
    }
    
    #[test]
    fn pt_rle_counts_positive(
        data in proptest::collection::vec(any::<u8>(), 1..100)
    ) {
        let encoded = rle_encode(&data);
        for &(_, count) in &encoded {
            prop_assert!(count > 0, "count is 0");
        }
    }
    
    // EXTRA: propiedad con regex (solo posible en proptest)
    #[test]
    fn pt_rle_compression_ratio(
        // Datos repetitivos: deberían comprimirse bien
        byte in any::<u8>(),
        count in 2..200usize
    ) {
        let data: Vec<u8> = vec![byte; count];
        let encoded = rle_encode(&data);
        // Un run homogéneo debería comprimirse a pocos pares
        prop_assert!(
            encoded.len() <= (count / 255) + 1,
            "compresión ineficiente: {} runs para {} bytes iguales",
            encoded.len(), count
        );
    }
}

// ── Word frequency: propiedades ──────────────────────────────

proptest! {
    #[test]
    fn pt_freq_sum_equals_token_count(s in any::<String>()) {
        let freq = word_frequency(&s);
        let sum: usize = freq.values().sum();
        let tokens = tokenize(&s);
        prop_assert_eq!(sum, tokens.len());
    }
    
    #[test]
    fn pt_freq_all_positive(s in any::<String>()) {
        for (&ref word, &count) in &word_frequency(&s) {
            prop_assert!(count > 0, "palabra '{}' tiene count 0", word);
        }
    }
    
    // EXTRA: propiedad con texto controlado
    #[test]
    fn pt_freq_known_words(
        words in proptest::collection::vec("[a-z]{2,6}", 1..20)
    ) {
        let text = words.join(" ");
        let freq = word_frequency(&text);
        
        // Cada palabra del input debe estar en el resultado
        for word in &words {
            prop_assert!(
                freq.contains_key(word),
                "palabra '{}' no está en frequencies", word
            );
        }
    }
}
```

### Ejecutar ambos

```bash
# Ejecutar todos los tests (ambos frameworks)
cargo test

# Solo quickcheck
cargo test --test with_quickcheck

# Solo proptest
cargo test --test with_proptest

# Con más iteraciones
QUICKCHECK_TESTS=1000 cargo test --test with_quickcheck
PROPTEST_CASES=1000 cargo test --test with_proptest
```

---

## 28. Ejercicios

### Ejercicio 1: Comparar calidad de shrinking

**Objetivo**: Observar la diferencia práctica en la calidad del shrinking.

**Tareas**:

**a)** Implementa una función `find_first_duplicate(v: &[i32]) -> Option<i32>` con un bug intencional (por ejemplo, que solo detecte duplicados adyacentes en vez de cualquier duplicado).

**b)** Escribe la propiedad "si insertamos un duplicado, find_first_duplicate lo encuentra" en AMBOS frameworks.

**c)** Corre ambos tests y compara:
- El caso mínimo que reporta quickcheck
- El caso mínimo que reporta proptest
- ¿Cuál es más fácil de entender para diagnosticar el bug?

**d)** Repite 3 veces y anota si quickcheck da el mismo caso mínimo las 3 veces. ¿Y proptest?

---

### Ejercicio 2: Migrar un test suite de quickcheck a proptest

**Objetivo**: Practicar la migración mecánica.

**Tareas**:

**a)** Dado este test suite en quickcheck, migralo completo a proptest:

```rust
use quickcheck::{quickcheck, Arbitrary, Gen, TestResult};

#[derive(Debug, Clone)]
struct Fraction { num: i32, den: i32 }

impl Arbitrary for Fraction {
    fn arbitrary(g: &mut Gen) -> Fraction {
        let num = i32::arbitrary(g);
        let den = loop {
            let d = i32::arbitrary(g);
            if d != 0 { break d; }
        };
        Fraction { num, den }
    }
}

quickcheck! {
    fn prop_add_comm(a: Fraction, b: Fraction) -> bool {
        // a + b == b + a (usando cross-multiplication)
        let lhs_num = a.num * b.den + b.num * a.den;
        let lhs_den = a.den * b.den;
        let rhs_num = b.num * a.den + a.num * b.den;
        let rhs_den = b.den * a.den;
        lhs_num * rhs_den == rhs_num * lhs_den
    }
    
    fn prop_mul_identity(a: Fraction) -> bool {
        // a * 1 == a
        let result_num = a.num * 1;
        let result_den = a.den * 1;
        result_num * a.den == a.num * result_den
    }
    
    fn prop_negate_involutive(a: Fraction) -> bool {
        // --a == a
        let neg_num = -(-a.num);
        neg_num == a.num
    }
}
```

**b)** Añade al menos 2 propiedades que serían difíciles o imposibles en quickcheck pero naturales en proptest (por ejemplo, usando rangos o regex).

**c)** Compara el código: ¿cuántas líneas ahorras? ¿Qué es más legible?

---

### Ejercicio 3: bolero como puente PBT-fuzzing

**Objetivo**: Explorar cómo bolero unifica PBT y fuzzing.

**Tareas**:

**a)** Agrega bolero a un proyecto:

```toml
[dev-dependencies]
bolero = "0.10"
```

**b)** Escribe un test que verifique que un parser no hace panic con input arbitrario, usando `check!().with_type::<Vec<u8>>()`.

**c)** Compara el mismo test escrito con proptest y con bolero. ¿Qué diferencias hay en la sintaxis y en el output?

**d)** Investiga: ¿cómo ejecutarías el mismo test como fuzzer con libfuzzer? (Documentar los pasos, no es necesario ejecutar si no tienes libfuzzer instalado.)

---

### Ejercicio 4: Diseñar una guía de decisión para tu equipo

**Objetivo**: Sintetizar lo aprendido en una guía práctica.

**Tareas**:

**a)** Crea un documento de una página que responda:
- ¿Qué framework de PBT usamos? (proptest/quickcheck/bolero)
- ¿Cuántas iteraciones por test? (default vs CI)
- ¿Commiteamos archivos .proptest-regressions?
- ¿Cuándo escribir PBT vs unit test vs ambos?
- ¿Cómo nombrar los tests de propiedad?

**b)** Para cada decisión, justifica con al menos un argumento técnico concreto (no "porque es mejor").

**c)** Incluye un ejemplo mínimo de test que sirva como plantilla para tu equipo.

---

## Navegación

- **Anterior**: [T03 - Diseñar propiedades](../T03_Disenar_propiedades/README.md)
- **Siguiente**: [S04/T01 - Trait-based dependency injection](../../S04_Mocking_en_Rust/T01_Trait_based_DI/README.md)

---

> **Sección 3: Property-Based Testing** — Completada (4 de 4 tópicos)
>
> - T01: Concepto — generación aleatoria, propiedades invariantes, shrinking ✓
> - T02: proptest — proptest!, any::\<T\>(), estrategias, ProptestConfig, regex ✓
> - T03: Diseñar propiedades — round-trip, invariantes, oracle, idempotencia, algebraicas ✓
> - T04: proptest vs quickcheck — diferencias de API, shrinking, cuándo usar cada uno ✓
>
> **Fin de la Sección 3: Property-Based Testing**
>
> Esta sección cubrió el paradigma de property-based testing desde los fundamentos conceptuales hasta la comparación práctica de frameworks. El estudiante ahora puede:
> - Pensar en propiedades universales en lugar de ejemplos concretos
> - Usar proptest con estrategias, composición, regex y configuración avanzada
> - Diseñar propiedades efectivas usando la taxonomía completa (round-trip, algebraicas, oracle, idempotencia, preservación, negativas)
> - Elegir entre proptest y quickcheck según las necesidades del proyecto
