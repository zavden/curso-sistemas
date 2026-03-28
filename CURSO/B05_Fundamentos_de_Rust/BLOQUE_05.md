# Bloque 5: Fundamentos de Rust

**Objetivo**: Dominar Rust desde cero hasta nivel intermedio, con énfasis en el
sistema de ownership y los patrones idiomáticos del lenguaje.

## Capítulo 1: Toolchain y Ecosistema [A]

### Sección 1: Herramientas
- **T01 - rustup**: instalación, toolchains (stable/nightly/beta), targets, components
- **T02 - Cargo**: new, build, run, test, check, doc — estructura de proyecto
- **T03 - Cargo.toml**: [package], [dependencies], features, [profile.dev/release]
- **T04 - Herramientas auxiliares**: rustfmt, clippy, cargo-expand, rust-analyzer

### Sección 2: El Ecosistema
- **T01 - crates.io**: publicación, versionado semántico, yanking
- **T02 - Documentación**: cargo doc, doc comments (///), módulo-level (//!), ejemplos en docs que se compilan

## Capítulo 2: Tipos de Datos y Variables [A]

### Sección 1: Tipos Primitivos
- **T01 - Enteros**: i8-i128, u8-u128, isize/usize — overflow (panic en debug, wrap en release)
- **T02 - Flotantes**: f32, f64 — NaN, Infinity, comparaciones (¡f64 no implementa Eq!)
- **T03 - Booleanos y char**: bool, char (4 bytes, Unicode scalar value, no es lo mismo que u8)
- **T04 - Inferencia de tipos**: cuándo anotar, turbofish (::<>), ambigüedad

### Sección 2: Variables y Mutabilidad
- **T01 - let vs let mut**: inmutabilidad por defecto, re-binding vs mutación
- **T02 - Shadowing**: re-declarar con el mismo nombre, cambio de tipo, scope
- **T03 - Constantes**: const vs static, evaluación en compilación, static mut (unsafe)
- **T04 - Tipo unit ()**: qué es, cuándo aparece, por qué importa

### Sección 3: Tipos Compuestos
- **T01 - Tuplas**: acceso por índice, destructuring, tupla vacía = ()
- **T02 - Arrays**: tamaño fijo, [T; N], inicialización, acceso con bounds checking
- **T03 - Slices**: &[T], &str, fat pointers (puntero + longitud), relación con arrays

## Capítulo 3: Ownership, Borrowing y Lifetimes [A]

### Sección 1: Ownership
- **T01 - Las 3 reglas**: un dueño, un solo dueño a la vez, drop al salir de scope
- **T02 - Move semántica**: por qué String se mueve pero i32 se copia, Copy vs Clone
- **T03 - Drop**: el trait Drop, orden de destrucción, drop manual con std::mem::drop
- **T04 - Copy y Clone**: cuándo derivar, cuándo no, tipos que no pueden ser Copy

### Sección 2: Borrowing
- **T01 - Referencias compartidas (&T)**: múltiples lectores, sin mutación
- **T02 - Referencias mutables (&mut T)**: exclusividad, no aliasing
- **T03 - Reglas del borrow checker**: por qué una ref mut excluye refs compartidas
- **T04 - Reborrowing**: por qué funciona pasar &mut a una función sin mover la referencia

### Sección 3: Lifetimes
- **T01 - Lifetime elision**: las 3 reglas del compilador, cuándo anotar manualmente
- **T02 - Anotaciones explícitas**: 'a, múltiples lifetimes, relación entre ellos
- **T03 - Lifetime en structs**: structs que contienen referencias
- **T04 - 'static**: qué significa realmente, string literals, T: 'static no significa "vive para siempre"

## Capítulo 4: Control de Flujo y Pattern Matching [A]

### Sección 1: Control de Flujo
- **T01 - if/else como expresión**: retornar valores, if let
- **T02 - Loops**: loop, while, for, break con valor, labels para loops anidados
- **T03 - Iteradores**: .iter(), .into_iter(), .iter_mut(), for vs iteradores explícitos

### Sección 2: Pattern Matching
- **T01 - match**: exhaustividad obligatoria, destructuring, guards
- **T02 - Patrones**: literales, variables, wildcard, rangos, ref, bindings (@)
- **T03 - if let y while let**: match de una sola rama, cuándo preferir sobre match
- **T04 - let-else**: Rust 1.65+, early return con pattern match

## Capítulo 5: Structs, Enums y Traits [A]

### Sección 1: Structs
- **T01 - Struct con campos nombrados**: definición, instanciación, field init shorthand
- **T02 - Tuple structs y unit structs**: newtype pattern, marker types
- **T03 - Métodos e impl blocks**: &self, &mut self, self, Self, métodos asociados
- **T04 - Visibilidad**: pub en campos, módulo como boundary de encapsulación

### Sección 2: Enums
- **T01 - Enums con datos**: variantes con campos nombrados, tuplas, unit
- **T02 - Option<T>**: Some, None, por qué no existe null, métodos (unwrap, map, and_then)
- **T03 - Result<T, E>**: Ok, Err, propagación con ?, From para conversión de errores

### Sección 3: Traits
- **T01 - Definición e implementación**: trait, impl Trait for Type, métodos default
- **T02 - Traits estándar clave**: Display, Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash
- **T03 - Derive macro**: #[derive(...)], cuándo funciona, cuándo implementar manualmente
- **T04 - Trait bounds**: fn foo<T: Trait>, where clauses, múltiples bounds (+)
- **T05 - Trait objects**: dyn Trait, vtable, &dyn vs Box<dyn>, object safety

## Capítulo 6: Manejo de Errores [A]

### Sección 1: Estrategias
- **T01 - panic! vs Result**: cuándo usar cada uno, unwinding vs abort
- **T02 - Propagación con ?**: conversión automática con From, retorno temprano
- **T03 - Tipos de error custom**: struct vs enum, implementar std::error::Error
- **T04 - thiserror y anyhow**: ecosistema de manejo de errores, cuándo usar cada uno

## Capítulo 7: Colecciones [A]

### Sección 1: Colecciones Estándar
- **T01 - Vec<T>**: push, pop, indexado, capacidad vs longitud, layout en memoria
- **T02 - String y &str**: UTF-8, por qué no se puede indexar por posición, chars() vs bytes()
- **T03 - HashMap<K, V>**: inserción, entry API, hashing, por qué K necesita Hash+Eq
- **T04 - HashSet, BTreeMap, BTreeSet, VecDeque**: cuándo preferir cada uno

## Capítulo 8: Genéricos e Iteradores [A]

### Sección 1: Genéricos
- **T01 - Funciones genéricas**: sintaxis, monomorphization, costo en tamaño de binario
- **T02 - Structs y enums genéricos**: Option<T>, Result<T,E> como ejemplos
- **T03 - impl para tipos genéricos**: impl<T> Foo<T>, impl Foo<i32>, blanket impls

### Sección 2: Closures
- **T01 - Sintaxis y captura**: |args| expr, captura por referencia vs por valor, inferencia de tipos
- **T02 - Fn, FnMut, FnOnce**: los 3 traits de closure, cuándo se infiere cada uno, jerarquía
- **T03 - move closures**: keyword move, transferencia de ownership, uso con threads y async
- **T04 - Closures como parámetros y retorno**: impl Fn vs dyn Fn vs genéricos, boxing closures, Fn trait bounds

### Sección 3: Iteradores
- **T01 - El trait Iterator**: Item, next(), por qué son lazy
- **T02 - Adaptadores**: map, filter, enumerate, zip, take, skip, chain, flatten
- **T03 - Consumidores**: collect, sum, count, any, all, find, fold/reduce
- **T04 - Iteradores custom**: implementar Iterator para tipos propios, IntoIterator

## Capítulo 9: Smart Pointers [A]

### Sección 1: Punteros al Heap
- **T01 - Box<T>**: heap allocation, cuándo es necesario, tipos recursivos (listas enlazadas, árboles)
- **T02 - Deref y DerefMut**: coerción automática, implementar Deref para tipos propios, deref coercion chain
- **T03 - Drop revisitado**: orden de destrucción en structs, std::mem::drop vs scope, ManuallyDrop

### Sección 2: Conteo de Referencias
- **T01 - Rc<T>**: reference counting single-thread, Rc::clone, strong_count, cuándo usar
- **T02 - Arc<T>**: Rc atómico para multi-thread, overhead vs Rc, por qué no usar Arc siempre
- **T03 - Weak<T>**: referencias débiles, romper ciclos, upgrade/downgrade, ejemplo de grafo/árbol con padre

### Sección 3: Interior Mutability
- **T01 - Cell<T>**: get/set sin referencia mutable, solo para Copy types, zero-cost
- **T02 - RefCell<T>**: borrowing dinámico, borrow/borrow_mut, panic en runtime si se viola
- **T03 - Rc<RefCell<T>>**: el patrón completo para datos compartidos mutables single-thread
- **T04 - OnceCell y LazyCell**: inicialización perezosa, std::sync::OnceLock para multi-thread

### Sección 4: Cow y Otros
- **T01 - Cow<T>**: clone-on-write, optimización de strings y slices, Cow<'a, str> en APIs
- **T02 - Pin<T>**: por qué existe, self-referential structs, relación con async/Future
- **T03 - Cuándo usar cada smart pointer**: árbol de decisión práctico

## Capítulo 10: Módulos y Crates [A]

### Sección 1: Sistema de Módulos
- **T01 - mod y archivos**: mod.rs vs nombre_modulo.rs (edición 2018+), rutas
- **T02 - Visibilidad**: pub, pub(crate), pub(super), pub(in path)
- **T03 - use y re-exports**: use, pub use, as, glob imports ({...}, *)
- **T04 - El preludio**: qué incluye, cómo crear uno propio

### Sección 2: Workspace y Crates Múltiples
- **T01 - Workspaces**: Cargo.toml raíz, members, dependencias compartidas
- **T02 - Crate de librería vs binario**: lib.rs vs main.rs, ambos en un crate
- **T03 - Features**: features opcionales, compilación condicional con cfg

### Sección 3: Macros Declarativas
- **T01 - macro_rules! básico**: sintaxis, matchers ($x:expr, $x:ident, $x:ty), repetición ($(...),*)
- **T02 - Macros del ecosistema**: cómo leer vec![], println![], assert_eq![] — expandir con cargo-expand
- **T03 - Cuándo escribir una macro vs una función genérica**: tradeoffs, debugging de macros, errores crípticos

## Capítulo 11: Testing [A]

### Sección 1: Tests en Rust
- **T01 - Unit tests**: #[test], #[cfg(test)], assert!, assert_eq!, assert_ne!
- **T02 - Tests de integración**: carpeta tests/, scope, #[ignore]
- **T03 - Tests que esperan panic**: #[should_panic], expected message
- **T04 - Doc tests**: ejemplos en documentación que se ejecutan como tests, `no_run`, `ignore`
