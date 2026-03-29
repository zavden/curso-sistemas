# Bloque 16: Patrones de Diseño en C y Rust

**Objetivo**: Dominar los patrones de diseño clásicos y modernos aplicados a
programación de sistemas. Cada patrón se implementa primero en C (mecanismo
explícito: function pointers, void\*, structs) y luego en Rust (abstracción
segura: traits, enums, generics, closures).

---

## Ubicación en la ruta de estudio

```
B01 → B02 → B03 → B04 → B05 → B15 → ★ B16 ★
       Docker  Linux    C     Build  Rust   ED      Patrones
```

### Prerequisitos obligatorios: B01–B05

| Bloque | Aporta a B16 |
|--------|-------------|
| B01 (Docker) | Entorno de laboratorio con gcc, cargo, valgrind |
| B02 (Linux) | Modelo de procesos, filesystem, permisos |
| B03 (C) | Structs, punteros a función, void\*, malloc/free, macros, bibliotecas |
| B04 (Build) | Make/CMake para proyectos multi-archivo |
| B05 (Rust) | Traits, generics, enums, closures, Box/Rc/RefCell, iteradores |

### Prerequisito recomendado: B15

B15 (Estructuras de Datos) no es estrictamente obligatorio, pero es
**fuertemente recomendado** porque:

1. **Práctica dual C/Rust.** B15 establece el flujo "implementar en C → portar
   a Rust" que B16 asume como natural.
2. **Patrones emergentes.** Varios patrones de B16 (Iterator, Visitor,
   Composite) aplican directamente sobre estructuras de datos de B15.
3. **Punteros opacos y vtables.** B15 C01 introduce opaque types y dispatch
   manual en C, que son la base mecánica de los patrones estructurales.
4. **unsafe Rust.** El mini-módulo de unsafe en B15 C01 S04 prepara para los
   patrones que requieren raw pointers en Rust.

**Sin B15:** Se puede empezar B16 directamente después de B05, pero los
capítulos C04 (Comportamiento) y C05 (Sistemas) serán más difíciles sin haber
implementado listas enlazadas, árboles, y tablas hash antes.

### Bloques opcionales que enriquecen

| Bloque | Enriquece |
|--------|-----------|
| B06 (Sistemas en C) | C05 (patrones de recursos: fd, mmap, señales) y C06 (concurrencia) |
| B07 (Sistemas en Rust) | C06 (async, channels, Arc) y C07 (patrones idiomáticos avanzados) |
| B14 (Interop C/Rust) | C05 S03 (API boundaries entre C y Rust) |

---

## Filosofía del bloque

```
GoF clásico:  23 patrones pensados para Java/C++ con herencia y clases.
Este bloque:  Patrones adaptados a C y Rust, lenguajes sin herencia.

En C:    Los patrones se construyen con structs + function pointers + void*.
         No hay polimorfismo nativo → lo construyes a mano (vtable, callbacks).
         Ves exactamente qué hace el compilador de C++ o Rust por debajo.

En Rust: Los patrones emergen del sistema de tipos.
         Traits = interfaces, enums = sum types, closures = estrategias.
         Muchos patrones GoF "desaparecen" porque el lenguaje los absorbe.

Lección central: un patrón es una solución a un problema recurrente,
no una receta que se aplica ciegamente. Si el lenguaje ya lo resuelve,
no lo reimplementes.
```

---

## Capítulo 1: Fundamentos — Polimorfismo sin Clases [A]

### Sección 1: Polimorfismo en C
- **T01 - Function pointers como estrategia**: typedef, declaración, invocación, arrays de function pointers
- **T02 - void\* como tipo genérico**: casting, alignment, limitaciones, convenciones de ownership
- **T03 - Vtable manual**: struct de function pointers, dispatch dinámico hecho a mano
- **T04 - Opaque pointer (handle pattern)**: forward declaration, struct incompleto, API pública vs implementación privada

### Sección 2: Polimorfismo en Rust
- **T01 - Trait objects (dyn Trait)**: dispatch dinámico, Box<dyn Trait>, vtable generada por el compilador
- **T02 - Generics + trait bounds**: dispatch estático, monomorphization, cuándo preferir sobre dyn
- **T03 - Enums como sum types**: pattern matching exhaustivo, data + behavior en un solo tipo
- **T04 - Closures como estrategia**: Fn/FnMut/FnOnce, captura por referencia vs por valor, Box<dyn Fn>

### Sección 3: C vs Rust — Trade-offs
- **T01 - Estático vs dinámico**: monomorphization vs vtable, tamaño de binario vs indirección
- **T02 - Cuándo usar cada mecanismo**: regla de decisión (conocido en compilación → generics; desconocido → dyn/void\*)
- **T03 - Costo real del dispatch dinámico**: cache misses, branch prediction, benchmarks comparativos

## Capítulo 2: Patrones Creacionales [A]

### Sección 1: Factory
- **T01 - Simple Factory en C**: función que retorna struct según parámetro, malloc + init
- **T02 - Simple Factory en Rust**: función que retorna Box<dyn Trait> o enum variant
- **T03 - Abstract Factory en C**: struct de function pointers que agrupa constructores relacionados
- **T04 - Abstract Factory en Rust**: trait con métodos asociados, type families

### Sección 2: Builder
- **T01 - Builder en C**: struct de configuración + función build(), valores por defecto con designated initializers
- **T02 - Builder en Rust**: method chaining con self, patrón consume-and-return
- **T03 - Typestate Builder en Rust**: estados en el tipo genérico, compilador rechaza construcción incompleta
- **T04 - Builder vs config struct**: cuándo basta un struct con Default, cuándo el builder aporta valor

### Sección 3: Singleton y Object Pool
- **T01 - Singleton en C**: static + función de acceso, pthread_once para thread safety
- **T02 - Singleton en Rust**: std::sync::LazyLock (1.80+), OnceLock, por qué es un anti-pattern y cuándo es aceptable
- **T03 - Object Pool en C**: array pre-asignado + free list, evitar malloc en hot path
- **T04 - Object Pool en Rust**: Vec<Option<T>> + generación de handles, typed-arena crate

## Capítulo 3: Patrones Estructurales [M]

### Sección 1: Adapter y Wrapper
- **T01 - Adapter en C**: wrapper function que traduce una API a otra, opaque struct intermedio
- **T02 - Adapter en Rust**: impl Trait for ForeignType (newtype si es de otro crate)
- **T03 - Newtype pattern en Rust**: struct tuple de un campo, deref coercion, cuándo implementar Deref y cuándo no

### Sección 2: Decorator
- **T01 - Decorator en C**: struct que envuelve otro struct del mismo "tipo" (misma vtable), cadena de decoradores
- **T02 - Decorator en Rust**: struct que implementa el mismo trait y delega, wrapping con Box<dyn Trait>
- **T03 - Decorator funcional**: composición de closures, middleware pattern

### Sección 3: Composite
- **T01 - Composite en C**: nodo con array de hijos (void\*\* children), operaciones recursivas
- **T02 - Composite en Rust**: enum recursivo con Box (árbol de expresiones, filesystem virtual)
- **T03 - Iteración sobre composites**: recorrido DFS/BFS, aplanamiento lazy

### Sección 4: Facade
- **T01 - Facade en C**: header público minimal que oculta complejidad interna, módulos internos con static linkage
- **T02 - Facade en Rust**: módulo público con re-exports selectivos, pub(crate), API pública vs interna

## Capítulo 4: Patrones de Comportamiento [M]

### Sección 1: Strategy
- **T01 - Strategy en C**: function pointer como campo de struct, intercambio en runtime
- **T02 - Strategy en Rust**: trait object o closure, comparación de ergonomía
- **T03 - Strategy estático en Rust**: generic con trait bound, zero-cost abstraction

### Sección 2: Observer
- **T01 - Observer en C**: lista de callbacks (function pointer + void\* context), register/unregister/notify
- **T02 - Observer en Rust**: Vec<Box<dyn Fn(Event)>>, variante con channels (mpsc)
- **T03 - Event bus**: despacho por tipo de evento, registro tipado

### Sección 3: State Machine
- **T01 - State Machine en C**: enum de estados + switch + tabla de transiciones, función de transición pura
- **T02 - State Machine en Rust**: enum con datos por variante + match exhaustivo
- **T03 - Typestate Machine en Rust**: estados como tipos, transiciones ilegales son errores de compilación

### Sección 4: Iterator
- **T01 - Iterator en C**: struct con estado + función next() que retorna puntero o NULL
- **T02 - Iterator en Rust**: impl Iterator, Item, adaptors (map, filter, take), lazy evaluation
- **T03 - Iterator interno vs externo**: callback-based (C idiomático) vs pull-based (Rust idiomático)

### Sección 5: Command y Visitor
- **T01 - Command en C**: struct con function pointer execute + void\* context, cola de comandos, undo
- **T02 - Command en Rust**: enum de comandos + match, o Box<dyn Fn()>, historial con Vec
- **T03 - Visitor en C**: function pointer que recorre estructura, double dispatch manual
- **T04 - Visitor en Rust**: por qué enum + match reemplaza Visitor en la mayoría de casos, cuándo sí usar trait Visitor

## Capítulo 5: Patrones de Sistemas y Recursos [M]

### Sección 1: Gestión de Recursos
- **T01 - RAII en C**: patrón init/destroy, goto cleanup, \_\_attribute\_\_((cleanup))
- **T02 - RAII en Rust**: Drop trait, ownership = gestión automática, scopeguard crate
- **T03 - Handle pattern**: entero como referencia indirecta, validación de generación (generational index)
- **T04 - Arena allocation**: bump allocator, lifetime unificado, liberar todo de golpe

### Sección 2: Error Handling Patterns
- **T01 - Error codes en C**: return int + errno, out-parameters, convención < 0 error
- **T02 - Result<T, E> en Rust**: el operador ?, From para conversión, thiserror vs anyhow
- **T03 - Error context enrichment**: cadena de errores, contexto por capa (archivo:línea → módulo → usuario)

### Sección 3: API Design Patterns
- **T01 - API opaca en C**: struct incompleto + constructor/destructor, versionado ABI
- **T02 - API ergonómica en Rust**: builder, Into<T>, impl AsRef, convenciones del ecosistema
- **T03 - C-compatible API desde Rust**: #[no_mangle], extern "C", cbindgen, cuándo diseñar la API en C primero

## Capítulo 6: Patrones de Concurrencia [E]

### Sección 1: Datos Compartidos
- **T01 - Mutex wrapper en C**: pthread_mutex + struct protegido, init/lock/unlock/destroy
- **T02 - Mutex<T> en Rust**: lock retorna MutexGuard (RAII), poisoning, por qué no puedes olvidar unlock
- **T03 - Reader-Writer lock**: pthread_rwlock en C, RwLock<T> en Rust, cuándo conviene sobre mutex

### Sección 2: Message Passing
- **T01 - Pipe/queue en C**: ring buffer con mutex, producer/consumer, condvar para espera
- **T02 - Channels en Rust**: mpsc, crossbeam-channel, select, backpressure
- **T03 - Actor-lite**: un thread + un channel receptor, procesamiento secuencial de mensajes

### Sección 3: Patrones Lock-Free (overview)
- **T01 - Atomic operations**: stdatomic.h en C, std::sync::atomic en Rust, memory orderings
- **T02 - Compare-and-swap idiom**: retry loop, ABA problem, cuándo vale la pena
- **T03 - Trade-offs**: lock-free no es siempre más rápido, contention vs throughput, cuándo elegir cada approach

## Capítulo 7: Anti-Patrones y Refactoring [A]

### Sección 1: Anti-Patrones Comunes
- **T01 - God struct / God module**: struct que hace todo, cómo identificar y dividir responsabilidades
- **T02 - Premature abstraction**: patrón sin problema, interface con una sola implementación
- **T03 - Stringly-typed programming**: usar strings donde conviene un enum, pérdida de type safety
- **T04 - Cargo cult patterns**: copiar un patrón sin entender el problema que resuelve

### Sección 2: Refactoring Toward Patterns
- **T01 - Señales de que falta un patrón**: switch creciente → Strategy/State, callbacks dispersos → Observer, constructión compleja → Builder
- **T02 - Refactoring incremental**: extract function → extract struct → introduce trait/vtable, sin reescribir todo
- **T03 - Cuándo NO aplicar un patrón**: 3 líneas duplicadas no justifican una abstracción, YAGNI en sistemas

## Capítulo 8: Clean Code — Principios Adaptados a C y Rust [A]

### Sección 1: Nombres y Funciones
- **T01 - Naming conventions**: snake_case en C y Rust, prefijos de módulo en C (módulo_acción), cuándo un nombre es suficientemente descriptivo
- **T02 - Funciones pequeñas y enfocadas**: una función = una responsabilidad, cuándo extraer, cuándo no (el extremo de funciones de 3 líneas)
- **T03 - Parámetros y return values**: máximo razonable de parámetros, struct de configuración vs parámetros individuales, out-parameters en C vs tuplas/Result en Rust
- **T04 - Side effects explícitos**: funciones puras vs funciones con estado, por qué `const` y `&T` comunican intención

### Sección 2: SOLID Adaptado a Sistemas
- **T01 - Single Responsibility en C/Rust**: un archivo .c = un módulo = una responsabilidad, mod.rs, cuándo dividir
- **T02 - Open-Closed sin herencia**: extensión por function pointers (C), por traits (Rust), sin modificar código existente
- **T03 - Liskov en traits**: contratos de trait, por qué `impl Trait for T` debe respetar la semántica esperada
- **T04 - Interface Segregation**: headers mínimos en C (no exponer internos), traits pequeños en Rust (Read vs Read+Write+Seek)
- **T05 - Dependency Inversion**: depender de abstracciones (function pointer / trait), no de implementaciones concretas

### Sección 3: Comentarios, Documentación y Formateo
- **T01 - Comentarios que aportan**: por qué, no qué; documenta decisiones y contratos, no syntax
- **T02 - Doc comments en Rust**: ///, //!, ejemplos que compilan, cargo doc como contrato vivo
- **T03 - Doxygen en C**: @param, @return, @brief, generación de documentación, estilo de proyecto
- **T04 - Formateo consistente**: clang-format para C, rustfmt para Rust, .editorconfig, por qué no discutir estilo

### Sección 4: Code Smells en C y Rust
- **T01 - Smells en C**: funciones > 50 líneas, macros que ocultan control de flujo, cast excesivos, globals mutables
- **T02 - Smells en Rust**: `.unwrap()` en producción, `.clone()` por pereza, `unsafe` innecesario, over-engineering con traits
- **T03 - Métricas simples**: complejidad ciclomática, profundidad de anidación, longitud de función — umbrales prácticos
- **T04 - Linting automatizado**: clippy (Rust), cppcheck (C), integración con editor y CI

## Capítulo 9: Data-Oriented Design [M]

### Sección 1: Modelo de Memoria y Cache
- **T01 - Jerarquía de memoria**: L1/L2/L3, latencias reales, cache lines (64 bytes), por qué importa para diseño
- **T02 - Cache hits y misses**: spatial locality, temporal locality, prefetching, cómo el layout de datos afecta rendimiento
- **T03 - Medir cache behavior**: perf stat, cachegrind, contadores de hardware, interpretar resultados

### Sección 2: AoS vs SoA
- **T01 - Array of Structs (AoS)**: layout natural en C y Rust, cuándo es la opción correcta
- **T02 - Struct of Arrays (SoA)**: separar campos en arrays paralelos, acceso columnar, SIMD-friendly
- **T03 - Transformación AoS ↔ SoA en C**: implementación manual, macros de ayuda, gestión de índices
- **T04 - Transformación AoS ↔ SoA en Rust**: múltiples Vec, crate soa-derive, iteración con zip

### Sección 3: Entity-Component-System (ECS)
- **T01 - Concepto ECS**: entities como IDs, components como datos puros, systems como funciones
- **T02 - ECS minimalista en C**: arrays paralelos, bitmask de componentes, iteración por sistema
- **T03 - ECS minimalista en Rust**: HashMap<EntityId, Component>, o Vec<Option<T>>, sparse sets
- **T04 - Por qué ECS es data-oriented**: separación de datos y lógica, cache-friendly iteration, comparación con OOP

### Sección 4: Patrones Prácticos de Layout
- **T01 - Hot/cold splitting**: separar campos frecuentes de infrecuentes, reducir cache footprint
- **T02 - Pool allocation y contigüidad**: objetos del mismo tipo contiguos en memoria, arena por tipo
- **T03 - Indices en lugar de punteros**: ventajas (serialización, reubicación, bounds checking), generational indices
- **T04 - Benchmark guiado**: medir antes de optimizar, caso práctico AoS vs SoA con criterion/perf
