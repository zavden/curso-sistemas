# T03 - Cobertura realista: qué medir, qué excluir y dónde enfocar

## Índice

1. [El problema del 100%](#1-el-problema-del-100)
2. [Cobertura como señal, no como objetivo](#2-cobertura-como-señal-no-como-objetivo)
3. [Las tres zonas de un proyecto](#3-las-tres-zonas-de-un-proyecto)
4. [Zona 1: código crítico — testear exhaustivamente](#4-zona-1-código-crítico--testear-exhaustivamente)
5. [Zona 2: código de soporte — testear razonablemente](#5-zona-2-código-de-soporte--testear-razonablemente)
6. [Zona 3: código no testeable — excluir conscientemente](#6-zona-3-código-no-testeable--excluir-conscientemente)
7. [Anatomía de main(): por qué no se testea](#7-anatomía-de-main-por-qué-no-se-testea)
8. [Patrón thin main](#8-patrón-thin-main)
9. [Error paths triviales: el código que existe "por si acaso"](#9-error-paths-triviales-el-código-que-existe-por-si-acaso)
10. [Clasificación de error paths](#10-clasificación-de-error-paths)
11. [Display y Debug implementations](#11-display-y-debug-implementations)
12. [Código generado y derives](#12-código-generado-y-derives)
13. [FFI bindings y wrappers de sistema](#13-ffi-bindings-y-wrappers-de-sistema)
14. [Mecanismos de exclusión en Rust](#14-mecanismos-de-exclusión-en-rust)
15. [Estrategia de exclusión con tarpaulin](#15-estrategia-de-exclusión-con-tarpaulin)
16. [Estrategia de exclusión con llvm-cov](#16-estrategia-de-exclusión-con-llvm-cov)
17. [Critical path analysis: identificar qué importa](#17-critical-path-analysis-identificar-qué-importa)
18. [Mapeo de rutas críticas](#18-mapeo-de-rutas-críticas)
19. [Coverage por módulo: la métrica que importa](#19-coverage-por-módulo-la-métrica-que-importa)
20. [Umbrales diferenciados por zona](#20-umbrales-diferenciados-por-zona)
21. [El anti-patrón del test cosmético](#21-el-anti-patrón-del-test-cosmético)
22. [Mutation testing: cuando la cobertura miente](#22-mutation-testing-cuando-la-cobertura-miente)
23. [cargo-mutants: verificar que los tests detectan bugs](#23-cargo-mutants-verificar-que-los-tests-detectan-bugs)
24. [Comparación con C y Go](#24-comparación-con-c-y-go)
25. [Errores comunes](#25-errores-comunes)
26. [Ejemplo completo: sistema de facturación](#26-ejemplo-completo-sistema-de-facturación)
27. [Programa de práctica: inventory_pricing](#27-programa-de-práctica-inventory_pricing)
28. [Ejercicios](#28-ejercicios)

---

## 1. El problema del 100%

La meta de 100% de cobertura de líneas suena virtuosa pero es contraproducente. Para
entender por qué, observa este código:

```rust
// src/main.rs
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <input>", args[0]);
        std::process::exit(1);   // ← ¿cómo testeas exit(1)?
    }
    let config = Config::from_file(&args[1])
        .unwrap_or_else(|e| {
            eprintln!("Error: {e}");
            std::process::exit(1);  // ← ¿y este?
        });
    run(config).unwrap_or_else(|e| {
        eprintln!("Fatal: {e}");
        std::process::exit(1);    // ← ¿y este también?
    });
}
```

Para cubrir estas líneas necesitarías:

1. Ejecutar el binario como proceso hijo (`Command::new`)
2. Capturar stderr y exit code
3. Verificar cada rama de error

¿Qué compraste con ese esfuerzo? Tres tests que verifican que `eprintln!` imprime un
mensaje y `exit(1)` sale con código 1 — cosas que el compilador ya garantiza. Mientras
tanto, la lógica real en `Config::from_file` y `run` podría tener cero tests.

```
┌──────────────────────────────────────────────────────────┐
│           El espejismo del 100%                          │
│                                                          │
│  100% cobertura ≠ 100% confianza                         │
│                                                          │
│  Puedes tener 100% de líneas cubiertas con tests que:    │
│  • No verifican nada (sin assert!)                       │
│  • Verifican lo obvio (assert!(true))                    │
│  • Ignoran casos borde                                   │
│  • No testean combinaciones de inputs                    │
│                                                          │
│  Y puedes tener 60% de cobertura con tests que:          │
│  • Cubren todos los caminos críticos                     │
│  • Detectan regresiones reales                           │
│  • Verifican invariantes de negocio                      │
│  • Testean edge cases en código que importa              │
│                                                          │
│  La meta: máxima confianza con esfuerzo proporcional     │
└──────────────────────────────────────────────────────────┘
```

Investigaciones reales respaldan esto:

| Estudio / Fuente | Hallazgo |
|---|---|
| Inozemtseva & Holmes (2014) | Cobertura de líneas por encima del 70-80% no correlaciona con mayor detección de bugs |
| Google Testing Blog | Recomiendan 60% como aceptable, 75% como encomiable, 90% como ejemplar |
| Microsoft Research | Las pruebas de mutación revelan que muchos tests de alta cobertura no detectan errores reales |
| Rust ecosystem | Proyectos maduros (tokio, serde, rustc) no buscan 100% — priorizan tests de integración |

---

## 2. Cobertura como señal, no como objetivo

La cobertura de código es un **termómetro**, no una **medicina**:

```
┌─────────────────────────────────────────────────────────────┐
│                 Cobertura como diagnóstico                   │
│                                                             │
│  Cobertura BAJA en módulo X                                 │
│       │                                                     │
│       ▼                                                     │
│  ¿Es código crítico?                                        │
│       │          │                                          │
│      Sí         No                                          │
│       │          │                                          │
│       ▼          ▼                                          │
│  SEÑAL DE       ¿Es código                                  │
│  RIESGO ⚠       no testeable?                               │
│  Escribir        │         │                                │
│  tests          Sí        No                                │
│                  │         │                                │
│                  ▼         ▼                                │
│              Excluir   Evaluar si                           │
│              y docu-   vale la pena                         │
│              mentar    el esfuerzo                          │
│                                                             │
│  Cobertura ALTA en módulo Y                                 │
│       │                                                     │
│       ▼                                                     │
│  ¿Los tests son significativos?                             │
│       │           │                                         │
│      Sí          No                                         │
│       │           │                                         │
│       ▼           ▼                                         │
│  Bien ✓       FALSA                                         │
│               SEGURIDAD ⚠                                   │
│               Revisar calidad                               │
│               de los tests                                  │
└─────────────────────────────────────────────────────────────┘
```

Las dos trampas:

**Trampa 1: cobertura como KPI de rendimiento**
Si mides a los desarrolladores por cobertura, escribirán tests para subir el número,
no para detectar bugs. Resultado: miles de tests que no verifican nada útil.

**Trampa 2: cobertura como pase de calidad**
"El pipeline pasa con 80%, entonces el código es bueno." No — el 80% podría cubrir
getters/setters triviales mientras el parser de transacciones financieras tiene 0%.

La mentalidad correcta:

```
                  ┌─────────────────────────────┐
                  │   COBERTURA REALISTA         │
                  │                             │
                  │   1. Identifica qué importa │
                  │   2. Testea eso a fondo      │
                  │   3. Excluye lo trivial      │
                  │   4. Mide para descubrir     │
                  │      huecos, no para         │
                  │      presumir números        │
                  └─────────────────────────────┘
```

---

## 3. Las tres zonas de un proyecto

Todo proyecto de software se divide naturalmente en tres zonas desde la perspectiva
de testing:

```
┌──────────────────────────────────────────────────────────────────────┐
│                    Las tres zonas de testing                         │
│                                                                      │
│  ┌──────────────────────┐                                            │
│  │   ZONA 1: CRÍTICA    │  Lógica de negocio, cálculos,             │
│  │   Cobertura: 90%+    │  validaciones, transformaciones,           │
│  │   Branch: 80%+       │  estado, invariantes                      │
│  │   Mutation: sí       │                                            │
│  └──────────┬───────────┘                                            │
│             │                                                        │
│  ┌──────────▼───────────┐                                            │
│  │   ZONA 2: SOPORTE    │  Serialización, logging, configuración,   │
│  │   Cobertura: 60-80%  │  adaptadores de I/O, CLI parsing,         │
│  │   Branch: 50%+       │  formatting                               │
│  │   Mutation: selectivo│                                            │
│  └──────────┬───────────┘                                            │
│             │                                                        │
│  ┌──────────▼───────────┐                                            │
│  │   ZONA 3: EXCLUIR    │  main(), glue code, Display triviales,    │
│  │   Cobertura: N/A     │  error boilerplate, código generado,      │
│  │   Excluir de métricas│  FFI wrappers puros                       │
│  └──────────────────────┘                                            │
│                                                                      │
│  La distribución típica:                                             │
│  Zona 1: ~30% del código  →  ~70% del esfuerzo de testing           │
│  Zona 2: ~40% del código  →  ~25% del esfuerzo de testing           │
│  Zona 3: ~30% del código  →  ~5% del esfuerzo (exclusiones)         │
└──────────────────────────────────────────────────────────────────────┘
```

### Regla del retorno decreciente

```
  Confianza
  ganada
    │
    │          ╱ Zona 1: cada test
    │        ╱   aporta mucho
    │      ╱
    │    ╱
    │  ╱─────────── Zona 2: cada test
    │╱               aporta algo
    │─────────────────────────────── Zona 3: cada test
    │                                 aporta poco
    └────────────────────────────────────────────
                                     Esfuerzo →

  Esfuerzo invertido en testing
```

---

## 4. Zona 1: código crítico — testear exhaustivamente

El código crítico es aquel donde un bug tiene consecuencias graves: pérdida de datos,
cálculos financieros incorrectos, violaciones de seguridad, corrupción de estado.

### Identificadores de código crítico

| Señal | Ejemplo |
|---|---|
| Maneja dinero | Cálculo de precios, impuestos, descuentos |
| Maneja autenticación/autorización | Verificación de tokens, permisos |
| Transforma datos irreversiblemente | Migraciones, eliminaciones, encriptación |
| Mantiene invariantes de estado | Máquinas de estado, saldos, inventario |
| Parsea input externo | Deserialización de API, parseo de archivos |
| Toma decisiones de negocio | Motor de reglas, elegibilidad, scoring |
| Coordina operaciones atómicas | Sagas, transacciones distribuidas |

### Qué significa testear exhaustivamente

```rust
// Ejemplo: función de cálculo de descuento
// ZONA 1 — requiere cobertura exhaustiva

pub fn calculate_discount(
    price: f64,
    quantity: u32,
    customer_tier: CustomerTier,
    coupon: Option<&Coupon>,
) -> Result<Discount, DiscountError> {
    if price <= 0.0 {
        return Err(DiscountError::InvalidPrice(price));
    }
    if quantity == 0 {
        return Err(DiscountError::ZeroQuantity);
    }

    let base = price * quantity as f64;

    let tier_pct = match customer_tier {
        CustomerTier::Standard => 0.0,
        CustomerTier::Silver => 0.05,
        CustomerTier::Gold => 0.10,
        CustomerTier::Platinum => 0.15,
    };

    let coupon_pct = match coupon {
        Some(c) if c.is_expired() => return Err(DiscountError::ExpiredCoupon),
        Some(c) if c.min_purchase > base => return Err(DiscountError::MinNotMet {
            required: c.min_purchase,
            actual: base,
        }),
        Some(c) => c.discount_pct,
        None => 0.0,
    };

    // Regla de negocio: los descuentos no se acumulan, se toma el mayor
    let final_pct = tier_pct.max(coupon_pct);
    // Regla: descuento máximo 25%
    let capped_pct = final_pct.min(0.25);

    Ok(Discount {
        original: base,
        discount_amount: base * capped_pct,
        final_price: base * (1.0 - capped_pct),
        applied_pct: capped_pct,
    })
}
```

Tests exhaustivos para esta función:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // --- Happy paths por tier ---

    #[test]
    fn standard_customer_no_discount() {
        let result = calculate_discount(100.0, 1, CustomerTier::Standard, None).unwrap();
        assert_eq!(result.discount_amount, 0.0);
        assert_eq!(result.final_price, 100.0);
    }

    #[test]
    fn silver_gets_5_percent() {
        let result = calculate_discount(200.0, 1, CustomerTier::Silver, None).unwrap();
        assert!((result.applied_pct - 0.05).abs() < f64::EPSILON);
        assert!((result.final_price - 190.0).abs() < f64::EPSILON);
    }

    #[test]
    fn gold_gets_10_percent() {
        let result = calculate_discount(200.0, 1, CustomerTier::Gold, None).unwrap();
        assert!((result.discount_amount - 20.0).abs() < f64::EPSILON);
    }

    #[test]
    fn platinum_gets_15_percent() {
        let result = calculate_discount(200.0, 1, CustomerTier::Platinum, None).unwrap();
        assert!((result.applied_pct - 0.15).abs() < f64::EPSILON);
    }

    // --- Cupón ---

    #[test]
    fn coupon_overrides_tier_when_higher() {
        let coupon = Coupon {
            discount_pct: 0.20,
            min_purchase: 50.0,
            expiry: far_future(),
        };
        let result = calculate_discount(
            100.0, 1, CustomerTier::Silver, Some(&coupon)
        ).unwrap();
        // Silver = 5%, cupón = 20% → se aplica 20%
        assert!((result.applied_pct - 0.20).abs() < f64::EPSILON);
    }

    #[test]
    fn tier_overrides_coupon_when_higher() {
        let coupon = Coupon {
            discount_pct: 0.03,
            min_purchase: 0.0,
            expiry: far_future(),
        };
        let result = calculate_discount(
            100.0, 1, CustomerTier::Gold, Some(&coupon)
        ).unwrap();
        // Gold = 10%, cupón = 3% → se aplica 10%
        assert!((result.applied_pct - 0.10).abs() < f64::EPSILON);
    }

    // --- Cap del 25% ---

    #[test]
    fn discount_capped_at_25_percent() {
        let coupon = Coupon {
            discount_pct: 0.40,  // 40% — excede el cap
            min_purchase: 0.0,
            expiry: far_future(),
        };
        let result = calculate_discount(
            100.0, 1, CustomerTier::Platinum, Some(&coupon)
        ).unwrap();
        // 40% > 15% → se aplica 40%, pero cap a 25%
        assert!((result.applied_pct - 0.25).abs() < f64::EPSILON);
        assert!((result.final_price - 75.0).abs() < f64::EPSILON);
    }

    // --- Quantity ---

    #[test]
    fn quantity_multiplies_base_price() {
        let result = calculate_discount(10.0, 5, CustomerTier::Standard, None).unwrap();
        assert!((result.original - 50.0).abs() < f64::EPSILON);
    }

    // --- Error paths (todos son relevantes en Zona 1) ---

    #[test]
    fn negative_price_is_error() {
        let result = calculate_discount(-1.0, 1, CustomerTier::Standard, None);
        assert!(matches!(result, Err(DiscountError::InvalidPrice(p)) if p == -1.0));
    }

    #[test]
    fn zero_price_is_error() {
        let result = calculate_discount(0.0, 1, CustomerTier::Standard, None);
        assert!(matches!(result, Err(DiscountError::InvalidPrice(_))));
    }

    #[test]
    fn zero_quantity_is_error() {
        let result = calculate_discount(100.0, 0, CustomerTier::Standard, None);
        assert!(matches!(result, Err(DiscountError::ZeroQuantity)));
    }

    #[test]
    fn expired_coupon_is_error() {
        let coupon = Coupon {
            discount_pct: 0.20,
            min_purchase: 0.0,
            expiry: past(),
        };
        let result = calculate_discount(100.0, 1, CustomerTier::Standard, Some(&coupon));
        assert!(matches!(result, Err(DiscountError::ExpiredCoupon)));
    }

    #[test]
    fn coupon_min_purchase_not_met() {
        let coupon = Coupon {
            discount_pct: 0.20,
            min_purchase: 500.0,
            expiry: far_future(),
        };
        let result = calculate_discount(100.0, 1, CustomerTier::Standard, Some(&coupon));
        assert!(matches!(
            result,
            Err(DiscountError::MinNotMet { required: 500.0, actual: 100.0 })
        ));
    }
}
```

**Observación**: 13 tests para una sola función. Eso es apropiado para Zona 1. Cada
test verifica una decisión de negocio específica. Si alguien cambia la regla del cap
o la lógica de acumulación, un test fallará inmediatamente.

---

## 5. Zona 2: código de soporte — testear razonablemente

El código de soporte hace que el sistema funcione pero no contiene lógica de negocio
directa. Los bugs aquí causan inconveniencia, no pérdida de datos.

### Identificadores de código de soporte

| Señal | Ejemplo |
|---|---|
| Serialización/Deserialización | serde impls, JSON/YAML parsing |
| Logging y telemetría | Formateo de logs, métricas |
| Configuración | Leer archivos de config, env vars |
| CLI parsing | clap/structopt, argument validation |
| Adaptadores de I/O | Wrappers de filesystem, HTTP clients |
| Formatting | Display impls no triviales, reporting |
| Middleware | Rate limiting, CORS, compression |

### Estrategia para Zona 2

```rust
// Ejemplo: módulo de configuración
// ZONA 2 — testear los caminos principales, no cada detalle

#[derive(Debug, Deserialize)]
pub struct AppConfig {
    pub database_url: String,
    pub port: u16,
    pub log_level: LogLevel,
    pub max_connections: u32,
    pub features: Features,
}

#[derive(Debug, Deserialize)]
pub struct Features {
    pub enable_cache: bool,
    pub enable_metrics: bool,
    pub experimental_parser: bool,
}

impl AppConfig {
    pub fn from_file(path: &Path) -> Result<Self, ConfigError> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| ConfigError::IoError(path.to_path_buf(), e))?;
        let config: Self = toml::from_str(&content)
            .map_err(|e| ConfigError::ParseError(e.to_string()))?;
        config.validate()?;
        Ok(config)
    }

    fn validate(&self) -> Result<(), ConfigError> {
        if self.port == 0 {
            return Err(ConfigError::InvalidPort);
        }
        if self.max_connections == 0 {
            return Err(ConfigError::InvalidMaxConnections);
        }
        if self.database_url.is_empty() {
            return Err(ConfigError::MissingDatabaseUrl);
        }
        Ok(())
    }
}
```

Tests razonables (no exhaustivos) para Zona 2:

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::NamedTempFile;

    fn write_config(content: &str) -> NamedTempFile {
        let mut file = NamedTempFile::new().unwrap();
        write!(file, "{}", content).unwrap();
        file
    }

    #[test]
    fn valid_config_loads_correctly() {
        let file = write_config(r#"
            database_url = "postgres://localhost/mydb"
            port = 8080
            log_level = "info"
            max_connections = 10
            [features]
            enable_cache = true
            enable_metrics = false
            experimental_parser = false
        "#);
        let config = AppConfig::from_file(file.path()).unwrap();
        assert_eq!(config.port, 8080);
        assert_eq!(config.max_connections, 10);
        assert!(config.features.enable_cache);
    }

    #[test]
    fn missing_file_returns_io_error() {
        let result = AppConfig::from_file(Path::new("/nonexistent"));
        assert!(matches!(result, Err(ConfigError::IoError(_, _))));
    }

    #[test]
    fn malformed_toml_returns_parse_error() {
        let file = write_config("this is not [valid toml");
        let result = AppConfig::from_file(file.path());
        assert!(matches!(result, Err(ConfigError::ParseError(_))));
    }

    #[test]
    fn port_zero_is_rejected() {
        let file = write_config(r#"
            database_url = "postgres://localhost/mydb"
            port = 0
            log_level = "info"
            max_connections = 10
            [features]
            enable_cache = false
            enable_metrics = false
            experimental_parser = false
        "#);
        let result = AppConfig::from_file(file.path());
        assert!(matches!(result, Err(ConfigError::InvalidPort)));
    }

    // No testeamos CADA validación — con una es suficiente para
    // confirmar que validate() se llama. Las demás son triviales.
}
```

**Observación**: 4 tests para un módulo completo. No testeamos cada combinación de
campos de `Features`, ni cada error de validación individualmente. El test de
`port_zero` confirma que `validate()` se ejecuta; las otras validaciones son
estructuralmente idénticas.

---

## 6. Zona 3: código no testeable — excluir conscientemente

El código no testeable no es "código malo". Es código cuyo testing tiene un costo
desproporcionado respecto al valor que aporta.

### Catálogo completo de código Zona 3

```
┌──────────────────────────────────────────────────────────────────┐
│              Código que NO deberías testear                       │
│                                                                  │
│  1. main() y bootstrap                                           │
│     fn main() { ... }                                            │
│     Razón: glue code, ya testeado transitivamente                │
│                                                                  │
│  2. Error Display triviales                                      │
│     impl Display for MyError { write!("{}", self.msg) }          │
│     Razón: un format string no puede fallar                      │
│                                                                  │
│  3. From conversiones mecánicas                                  │
│     impl From<io::Error> for AppError { ... }                    │
│     Razón: el compilador verifica la conversión                  │
│                                                                  │
│  4. Derives automáticos                                          │
│     #[derive(Debug, Clone, Serialize)]                           │
│     Razón: código generado por el compilador/macros              │
│                                                                  │
│  5. Logging statements                                           │
│     log::info!("Processing item {}", id);                        │
│     Razón: side effect de observabilidad, no lógica              │
│                                                                  │
│  6. Constructores triviales                                      │
│     fn new(name: String) -> Self { Self { name } }               │
│     Razón: una asignación de campo no puede fallar               │
│                                                                  │
│  7. Getters/Setters puros                                        │
│     fn name(&self) -> &str { &self.name }                        │
│     Razón: una referencia no puede fallar                        │
│                                                                  │
│  8. FFI declarations (extern "C")                                │
│     extern "C" { fn strlen(s: *const c_char) -> usize; }        │
│     Razón: la declaración no es código ejecutable                │
│                                                                  │
│  9. Type aliases y newtypes sin lógica                           │
│     type UserId = u64;                                           │
│     struct Meters(f64);                                          │
│     Razón: definiciones de tipos, no comportamiento              │
│                                                                  │
│  10. Código de shutdown/cleanup                                  │
│      impl Drop for Connection { self.close() }                   │
│      Razón: difícil de testear, riesgo bajo                     │
│                                                                  │
│  11. Panics intencionales (unreachable!())                       │
│      _ => unreachable!("invalid state")                          │
│      Razón: por definición, no debería ejecutarse                │
│                                                                  │
│  12. Código behind feature flags deshabilitados                  │
│      #[cfg(feature = "experimental")]                            │
│      Razón: no se compila en la configuración actual             │
└──────────────────────────────────────────────────────────────────┘
```

### La pregunta clave

Antes de escribir un test, pregúntate:

```
¿Si este código tiene un bug, qué es lo peor que puede pasar?

  - "El programa no compila"  →  NO testear (el compilador lo cubre)
  - "Se imprime un log feo"   →  NO testear (cosmético)
  - "Se pierde dinero"        →  SÍ testear exhaustivamente
  - "Se corrompen datos"      →  SÍ testear exhaustivamente
  - "Sale un error confuso"   →  QUIZÁS testear (depende)
```

---

## 7. Anatomía de main(): por qué no se testea

Un `main()` típico en Rust tiene estas responsabilidades:

```rust
fn main() -> Result<(), Box<dyn Error>> {
    // 1. Parse de argumentos CLI
    let args = Args::parse();

    // 2. Inicialización de logging
    tracing_subscriber::init();

    // 3. Carga de configuración
    let config = Config::load(&args.config_path)?;

    // 4. Construcción del "mundo" (dependency injection manual)
    let db = Database::connect(&config.database_url)?;
    let cache = Redis::connect(&config.redis_url)?;
    let mailer = SmtpMailer::new(&config.smtp_config);

    // 5. Inyección y ejecución
    let app = App::new(db, cache, mailer);
    app.run()?;

    Ok(())
}
```

Cada línea de `main()` es una de dos cosas:

| Tipo | Ejemplo | Ya testeado en |
|---|---|---|
| Llamada a librería | `Args::parse()` | Tests del crate clap |
| Wiring (conectar componentes) | `App::new(db, cache, mailer)` | Tests de integración de App |
| Propagación de error | `?` | El compilador verifica que los tipos son compatibles |
| Configuración de infra | `tracing_subscriber::init()` | Tests de tracing |

Testear `main()` directamente significaría:
- Levantar una base de datos real
- Levantar un Redis real
- Configurar un servidor SMTP
- Todo eso para verificar que las piezas se conectan... lo cual ya hacen los tests
  de integración de `App`

---

## 8. Patrón thin main

La solución para mantener `main()` fuera de la cobertura sin perder confianza:

```rust
// src/main.rs — ZONA 3: excluir de cobertura
#[cfg(not(tarpaulin_include))]
fn main() {
    if let Err(e) = run() {
        eprintln!("Error: {e}");
        std::process::exit(1);
    }
}

// src/lib.rs — ZONA 1 o 2: incluir en cobertura
pub fn run() -> Result<(), AppError> {
    let args = Args::parse();
    let config = Config::load(&args.config_path)?;
    let app = build_app(config)?;
    app.execute()
}

pub fn build_app(config: Config) -> Result<App, AppError> {
    let db = Database::connect(&config.database_url)?;
    let cache = Cache::connect(&config.redis_url)?;
    Ok(App::new(db, cache))
}
```

### Comparación: main gordo vs thin main

```
┌─────────────────────────────────────────────────────────────────┐
│  ANTES: main() gordo                                            │
│                                                                 │
│  main.rs (80 líneas)                                            │
│  ├── parse args         ← no testeable sin Command::new         │
│  ├── init logging       ← side effect global                    │
│  ├── load config        ← duplicado si ya testeamos Config      │
│  ├── connect db         ← necesita DB real                      │
│  ├── connect cache      ← necesita cache real                   │
│  ├── create app         ← wiring                                │
│  ├── app.run()          ← ya tiene tests propios                │
│  └── handle errors      ← eprintln + exit                       │
│                                                                 │
│  Cobertura: 0% (no podemos testearlo)                           │
│  Impacto en métricas: baja la cobertura global artificialmente   │
│                                                                 │
│─────────────────────────────────────────────────────────────────│
│  DESPUÉS: thin main                                             │
│                                                                 │
│  main.rs (5 líneas, excluido de métricas)                       │
│  └── if let Err(e) = run() { exit(1) }                          │
│                                                                 │
│  lib.rs (75 líneas, incluido en métricas)                       │
│  ├── run()         ← testeable con mocks/fakes de DB/Cache      │
│  ├── build_app()   ← testeable con config fixture                │
│  └── toda la lógica ← completamente testeable                   │
│                                                                 │
│  Cobertura: 85%+ (la lógica real está en lib.rs)                │
│  Las métricas reflejan la realidad                              │
└─────────────────────────────────────────────────────────────────┘
```

### Thin main en C (comparación)

```c
/* main.c — ZONA 3 */
int main(int argc, char *argv[]) {
    int status = app_run(argc, argv);
    return status;
}

/* app.c — ZONA 1/2: testeable */
int app_run(int argc, char *argv[]) {
    Config cfg;
    if (config_parse(argc, argv, &cfg) != 0) return 1;
    if (app_init(&cfg) != 0) return 1;
    return app_execute(&cfg);
}
```

### Thin main en Go (comparación)

```go
// main.go — ZONA 3
func main() {
    if err := run(os.Args[1:]); err != nil {
        fmt.Fprintf(os.Stderr, "Error: %v\n", err)
        os.Exit(1)
    }
}

// run.go — ZONA 1/2: testeable
func run(args []string) error {
    cfg, err := loadConfig(args)
    if err != nil {
        return fmt.Errorf("config: %w", err)
    }
    app, err := buildApp(cfg)
    if err != nil {
        return fmt.Errorf("init: %w", err)
    }
    return app.Execute()
}
```

El patrón thin main es universal — misma filosofía en los tres lenguajes.

---

## 9. Error paths triviales: el código que existe "por si acaso"

Muchos error paths en Rust son mecánicos — existen porque el sistema de tipos lo
requiere, no porque haya lógica interesante:

```rust
// Error path trivial: solo envuelve el error de otra capa
impl From<std::io::Error> for AppError {
    fn from(e: std::io::Error) -> Self {
        AppError::Io(e)  // ← solo wrapping, sin lógica
    }
}

// Error path trivial: el ? operador hace la conversión
fn read_config(path: &Path) -> Result<Config, AppError> {
    let content = std::fs::read_to_string(path)?;  // ← From<io::Error> automático
    let config = toml::from_str(&content)?;         // ← From<toml::Error> automático
    Ok(config)
}

// Error path trivial: Display que solo delega
impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Io(e) => write!(f, "I/O error: {e}"),
            AppError::Parse(e) => write!(f, "Parse error: {e}"),
            AppError::Config(msg) => write!(f, "Config error: {msg}"),
        }
    }
}
```

**¿Qué podría fallar aquí?**
- `From<io::Error>`: el compilador verifica que los tipos coinciden. Si escribimos mal
  la variante, no compila.
- `?` operador: el compilador verifica que `From` existe. Si no, no compila.
- `Display`: `write!` no puede fallar para strings. El formato es cosmético.

Testear esto es verificar que el compilador funciona.

---

## 10. Clasificación de error paths

No todos los error paths son iguales. Esta clasificación ayuda a decidir qué testear:

```
┌──────────────────────────────────────────────────────────────────────┐
│             Clasificación de error paths                             │
│                                                                      │
│  TIPO A: Error path con lógica de recuperación                       │
│  ──────────────────────────────────────                              │
│  fn process(data: &[u8]) -> Result<Output, Error> {                  │
│      match parse(data) {                                             │
│          Ok(parsed) => transform(parsed),                            │
│          Err(e) if e.is_recoverable() => {                           │
│              log::warn!("Recoverable: {e}, using fallback");         │
│              Ok(Output::default())   // ← LÓGICA DE DECISIÓN        │
│          }                                                           │
│          Err(e) => Err(e.into()),                                    │
│      }                                                               │
│  }                                                                   │
│  VEREDICTO: TESTEAR — la decisión de recuperar es lógica de negocio  │
│                                                                      │
│  TIPO B: Error path con transformación de error                      │
│  ──────────────────────────────────────                              │
│  fn transfer(from: &Account, to: &Account, amount: f64)              │
│      -> Result<Receipt, TransferError>                               │
│  {                                                                   │
│      if from.balance < amount {                                      │
│          return Err(TransferError::InsufficientFunds {               │
│              available: from.balance,                                │
│              requested: amount,         // ← DATOS ESPECÍFICOS       │
│          });                                                         │
│      }                                                               │
│      // ...                                                          │
│  }                                                                   │
│  VEREDICTO: TESTEAR — los datos del error son parte del contrato     │
│                                                                      │
│  TIPO C: Error path de propagación pura                              │
│  ──────────────────────────────────────                              │
│  fn load(path: &Path) -> Result<Data, AppError> {                    │
│      let bytes = std::fs::read(path)?;    // ← solo ?                │
│      let data = serde_json::from_slice(&bytes)?;  // ← solo ?        │
│      Ok(data)                                                        │
│  }                                                                   │
│  VEREDICTO: NO TESTEAR — el ? es mecánico, From lo verifica rustc    │
│                                                                      │
│  TIPO D: Error path de Display/Debug                                 │
│  ──────────────────────────────────────                              │
│  impl Display for MyError {                                          │
│      fn fmt(&self, f: &mut Formatter) -> fmt::Result {               │
│          write!(f, "error: {}", self.message)                        │
│      }                                                               │
│  }                                                                   │
│  VEREDICTO: NO TESTEAR — formato cosmético                           │
└──────────────────────────────────────────────────────────────────────┘
```

### Resumen

| Tipo | ¿Testear? | Razón |
|---|---|---|
| A: Recuperación | **Sí** | La decisión de recuperar vs propagar es lógica de negocio |
| B: Error con datos | **Sí** | Los datos del error son parte del contrato público |
| C: Propagación `?` | **No** | Mecánico, verificado por el compilador |
| D: Display/Debug | **No** | Cosmético, no puede fallar |

---

## 11. Display y Debug implementations

### Cuándo SÍ testear Display

```rust
// Display con lógica condicional — TESTEAR
impl fmt::Display for ValidationReport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "Validation Report:")?;
        writeln!(f, "  Total fields: {}", self.total)?;
        writeln!(f, "  Valid: {}", self.valid)?;
        writeln!(f, "  Invalid: {}", self.invalid)?;

        if !self.errors.is_empty() {
            writeln!(f, "\nErrors:")?;
            for (field, error) in &self.errors {
                writeln!(f, "  - {field}: {error}")?;
            }
        }

        if self.invalid > self.total / 2 {
            writeln!(f, "\n⚠ WARNING: More than 50% of fields are invalid")?;
        }

        Ok(())
    }
}
```

Este `Display` tiene lógica condicional (la advertencia del 50%). Si alguien cambia
el umbral de `/ 2` a `/ 3`, debería fallar un test. **Esto es Zona 1**.

### Cuándo NO testear Display

```rust
// Display trivial — NO TESTEAR
impl fmt::Display for UserId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "user-{}", self.0)
    }
}

// Display que solo delega — NO TESTEAR
impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::NotFound(id) => write!(f, "not found: {id}"),
            Self::Unauthorized => write!(f, "unauthorized"),
            Self::Internal(e) => write!(f, "internal: {e}"),
        }
    }
}
```

Estos son format strings puros. El único "bug" posible es un typo en el texto, que
no es un bug funcional.

---

## 12. Código generado y derives

### Derives estándar

```rust
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct User {
    pub id: UserId,
    pub name: String,
    pub email: String,
}
```

Estos derives generan código que:
- Es producido por macros que tienen sus propios tests (en `std` o en `serde`)
- Si el derive tiene un bug, es un bug del compilador/crate, no de tu código
- Los derives se testean transitivamente cuando usas las structs en tests

**Excepción**: si implementas manualmente un trait que normalmente sería un derive
(por ejemplo, `PartialEq` con lógica custom), ese código SÍ necesita tests:

```rust
// PartialEq custom — TESTEAR
impl PartialEq for Transaction {
    fn eq(&self, other: &Self) -> bool {
        // Ignora timestamps, compara solo por ID y amount
        self.id == other.id && self.amount == other.amount
    }
}

#[test]
fn transactions_equal_regardless_of_timestamp() {
    let t1 = Transaction { id: 1, amount: 100.0, timestamp: 1000 };
    let t2 = Transaction { id: 1, amount: 100.0, timestamp: 2000 };
    assert_eq!(t1, t2);
}

#[test]
fn transactions_differ_by_amount() {
    let t1 = Transaction { id: 1, amount: 100.0, timestamp: 1000 };
    let t2 = Transaction { id: 1, amount: 200.0, timestamp: 1000 };
    assert_ne!(t1, t2);
}
```

### Macros procedurales propias

Si tu proyecto tiene macros procedurales que generan código, esas macros SÍ necesitan
tests (son Zona 1 del crate de la macro), pero el código que generan en otros crates
no necesita tests adicionales.

---

## 13. FFI bindings y wrappers de sistema

### Declaraciones FFI puras

```rust
// Declaraciones — NO son código ejecutable, no se testean
extern "C" {
    fn sqlite3_open(filename: *const c_char, ppDb: *mut *mut sqlite3) -> c_int;
    fn sqlite3_close(db: *mut sqlite3) -> c_int;
    fn sqlite3_exec(
        db: *mut sqlite3,
        sql: *const c_char,
        callback: Option<extern "C" fn(*mut c_void, c_int, *mut *mut c_char, *mut *mut c_char) -> c_int>,
        arg: *mut c_void,
        errmsg: *mut *mut c_char,
    ) -> c_int;
}
```

### Wrappers unsafe delgados

```rust
// Wrapper delgado — ZONA 3 (el unsafe es mecánico)
pub fn open(path: &str) -> Result<Database, SqliteError> {
    let c_path = CString::new(path).map_err(|_| SqliteError::InvalidPath)?;
    let mut db: *mut sqlite3 = std::ptr::null_mut();
    let rc = unsafe { sqlite3_open(c_path.as_ptr(), &mut db) };
    if rc != SQLITE_OK {
        return Err(SqliteError::OpenFailed(rc));
    }
    Ok(Database { ptr: db })
}
```

Este wrapper solo traduce la API de C a Rust. El comportamiento real está en SQLite,
que tiene su propia suite de tests con millones de test cases.

### Wrappers con lógica — SÍ testear

```rust
// Wrapper con lógica de retry y pooling — ZONA 1
pub fn open_with_retry(
    path: &str,
    max_retries: u32,
    backoff: Duration,
) -> Result<Database, SqliteError> {
    let mut last_err = None;
    for attempt in 0..max_retries {
        match open(path) {
            Ok(db) => {
                if attempt > 0 {
                    log::info!("Connected after {} retries", attempt);
                }
                return Ok(db);
            }
            Err(e) if e.is_transient() => {
                last_err = Some(e);
                std::thread::sleep(backoff * attempt);
            }
            Err(e) => return Err(e),
        }
    }
    Err(last_err.unwrap_or(SqliteError::MaxRetriesExceeded))
}
```

Aquí hay lógica de retry, backoff y clasificación de errores. Esto SÍ se testea
(con un fake de `open` que falla N veces y luego tiene éxito).

---

## 14. Mecanismos de exclusión en Rust

Rust tiene varios mecanismos para excluir código de las métricas de cobertura:

### Atributo `cfg` para tarpaulin

```rust
// Excluir una función completa
#[cfg(not(tarpaulin_include))]
fn main() {
    // ...
}

// Excluir un bloque
#[cfg(not(tarpaulin_include))]
impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(e) => write!(f, "I/O: {e}"),
            Self::Parse(e) => write!(f, "Parse: {e}"),
        }
    }
}

// Excluir un módulo entero
#[cfg(not(tarpaulin_include))]
mod cli_output {
    // formateo de output para terminal
}
```

### Atributo `tarpaulin::skip`

```rust
// Alternativa más explícita (solo tarpaulin)
#[tarpaulin::skip]
fn bootstrap_logging() {
    // ...
}
```

### Diferencia entre `cfg(not(tarpaulin_include))` y `#[tarpaulin::skip]`

| Aspecto | `cfg(not(tarpaulin_include))` | `#[tarpaulin::skip]` |
|---|---|---|
| Efecto | No compila el código durante tarpaulin | Compila pero no instrumenta |
| Seguridad | El código no compilado no se verifica | El código se compila normalmente |
| Uso recomendado | `main()` que no necesita existir en tests | Funciones que deben compilar pero no medir |
| Riesgo | Drift: el código excluido puede dejar de compilar | Ninguno |

**Recomendación**: usa `#[tarpaulin::skip]` por defecto. Solo usa `cfg(not(tarpaulin_include))`
para `main()` o código que realmente no debería compilarse durante tests.

### Para llvm-cov: exclusión por archivo

`cargo-llvm-cov` no tiene atributos en código. Las exclusiones son por archivo:

```bash
# Excluir archivos por patrón
cargo llvm-cov --ignore-filename-regex 'main\.rs|cli\.rs|generated'
```

---

## 15. Estrategia de exclusión con tarpaulin

### Archivo `.tarpaulin.toml` completo

```toml
[default]
# Archivos a excluir del reporte
exclude-files = [
    "src/main.rs",          # thin main
    "src/bin/*",            # binarios auxiliares
    "src/cli.rs",           # CLI output formatting
    "src/generated/*",      # código generado por macros/protobuf
    "benches/*",            # benchmarks
    "examples/*",           # ejemplos
]

# Otros flags
verbose = false
engine = "llvm"           # o "ptrace"
out = ["html", "lcov"]
run-types = ["Tests"]     # excluir doctests si son lentos
timeout = "300"

[ci]
# Perfil para CI con umbrales
inherits = "default"
fail-under = 70
out = ["lcov"]
```

### Patrón de exclusión en código

```rust
// src/lib.rs
// ---- Zona 1: lógica de negocio (NO excluir) ----
pub mod pricing;
pub mod inventory;
pub mod orders;

// ---- Zona 2: soporte (NO excluir, pero umbral más bajo) ----
pub mod config;
pub mod logging;

// ---- Zona 3: excluir ----
#[cfg(not(tarpaulin_include))]
pub mod cli_output;    // formateo de terminal

// Los Display triviales se marcan individualmente:
#[derive(Debug)]
pub enum AppError {
    Io(std::io::Error),
    Config(String),
    Business(BusinessError),
}

#[tarpaulin::skip]
impl std::fmt::Display for AppError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io(e) => write!(f, "I/O error: {e}"),
            Self::Config(msg) => write!(f, "Config: {msg}"),
            Self::Business(e) => write!(f, "Business: {e}"),
        }
    }
}
```

---

## 16. Estrategia de exclusión con llvm-cov

### Exclusión por archivos en la línea de comandos

```bash
# Excluir main.rs y todo el directorio generated/
cargo llvm-cov --ignore-filename-regex 'main\.rs$|generated/|cli_output\.rs$'

# Excluir tests del reporte (ya están excluidos por defecto, pero por claridad)
cargo llvm-cov --ignore-filename-regex 'tests/|test_'
```

### Script de CI con exclusiones

```bash
#!/bin/bash
# scripts/coverage.sh

EXCLUDE_PATTERN='main\.rs$|bin/|generated/|cli_output\.rs$|benches/'

cargo llvm-cov \
    --ignore-filename-regex "$EXCLUDE_PATTERN" \
    --fail-under-lines 75 \
    --fail-under-branches 50 \
    --html \
    --output-dir target/coverage

echo "Coverage report: target/coverage/html/index.html"
```

### Combinar con tarpaulin (cuando usas ambos en CI)

```yaml
# .github/workflows/coverage.yml
jobs:
  coverage-tarpaulin:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
      - run: cargo install cargo-tarpaulin
      - run: |
          cargo tarpaulin \
            --exclude-files 'src/main.rs' \
            --exclude-files 'src/bin/*' \
            --exclude-files 'src/generated/*' \
            --fail-under 70 \
            --out lcov \
            --output-dir target/tarpaulin

  coverage-llvm:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@stable
        with:
          components: llvm-tools-preview
      - uses: taiki-e/install-action@cargo-llvm-cov
      - run: |
          cargo llvm-cov \
            --ignore-filename-regex 'main\.rs$|bin/|generated/' \
            --fail-under-lines 75 \
            --fail-under-branches 50 \
            --lcov --output-path lcov.info
```

---

## 17. Critical path analysis: identificar qué importa

El "critical path" de un programa es la secuencia de operaciones donde los bugs
tienen mayor impacto. Identificarlo es el paso más importante de una estrategia
de cobertura realista.

### Método de identificación

```
┌──────────────────────────────────────────────────────────────────────┐
│            Identificación de rutas críticas                          │
│                                                                      │
│  Paso 1: Listar las operaciones de negocio principales               │
│  ─────────────────────────────────────────────────                   │
│  • "El usuario compra un producto"                                   │
│  • "El sistema calcula impuestos"                                    │
│  • "Se genera una factura"                                           │
│  • "Se procesa un reembolso"                                         │
│                                                                      │
│  Paso 2: Para cada operación, trazar el flujo de datos               │
│  ─────────────────────────────────────────────────                   │
│  compra → validar_stock → calcular_precio → aplicar_descuento        │
│        → calcular_impuesto → crear_orden → cobrar → confirmar        │
│                                                                      │
│  Paso 3: Marcar cada función en el flujo con su impacto              │
│  ─────────────────────────────────────────────────                   │
│  validar_stock:      ALTO (vender algo que no hay)                   │
│  calcular_precio:    ALTO (cobrar mal)                               │
│  aplicar_descuento:  ALTO (dar descuento equivocado)                 │
│  calcular_impuesto:  ALTO (problemas legales)                        │
│  crear_orden:        MEDIO (se puede corregir)                       │
│  cobrar:             CRÍTICO (dinero real)                           │
│  confirmar:          BAJO (se puede reenviar)                        │
│                                                                      │
│  Paso 4: Asignar nivel de testing                                    │
│  ─────────────────────────────────────────────────                   │
│  ALTO/CRÍTICO → Zona 1: tests exhaustivos + branch coverage         │
│  MEDIO        → Zona 2: tests razonables                            │
│  BAJO         → Zona 2/3: tests mínimos o excluir                   │
└──────────────────────────────────────────────────────────────────────┘
```

### Heurísticas para impacto

| Pregunta | Si la respuesta es "sí" | Nivel |
|---|---|---|
| ¿Maneja dinero real? | Zona 1, branch coverage | CRÍTICO |
| ¿Puede perder datos del usuario? | Zona 1, branch coverage | CRÍTICO |
| ¿Tiene implicaciones legales? | Zona 1, branch coverage | ALTO |
| ¿Afecta a muchos usuarios simultáneamente? | Zona 1 | ALTO |
| ¿Es difícil de revertir si falla? | Zona 1 | ALTO |
| ¿Causa downtime visible? | Zona 2 alta | MEDIO |
| ¿Genera datos incorrectos en reportes? | Zona 2 | MEDIO |
| ¿Solo afecta la estética del output? | Zona 3 | BAJO |
| ¿Solo afecta al desarrollador, no al usuario? | Zona 3 | BAJO |

---

## 18. Mapeo de rutas críticas

### Ejemplo: e-commerce

```
┌─────────────────────────────────────────────────────────────────────┐
│     Mapa de rutas críticas: E-commerce                              │
│                                                                     │
│  FLUJO: Checkout                                                    │
│  ═══════════════                                                    │
│                                                                     │
│  [Cart] ──→ [Validate] ──→ [Price] ──→ [Tax] ──→ [Pay] ──→ [Ship] │
│    Z2          Z1            Z1         Z1         Z1        Z2     │
│                                                                     │
│  FLUJO: Reembolso                                                   │
│  ════════════════                                                   │
│                                                                     │
│  [Request] ──→ [Validate] ──→ [Calculate] ──→ [Refund] ──→ [Notify]│
│     Z2            Z1             Z1             Z1           Z3     │
│                                                                     │
│  FLUJO: Inventario                                                  │
│  ════════════════                                                   │
│                                                                     │
│  [Receive] ──→ [Count] ──→ [Update] ──→ [Alert] ──→ [Report]      │
│     Z2           Z1          Z1           Z2          Z3            │
│                                                                     │
│  Leyenda:                                                           │
│  Z1 = Tests exhaustivos (90%+ line, 80%+ branch)                   │
│  Z2 = Tests razonables  (60-80% line)                               │
│  Z3 = Excluir o tests mínimos                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Documentar las decisiones

Es fundamental documentar **por qué** cada módulo está en su zona. Esto evita que
futuros desarrolladores agreguen tests innecesarios o eliminen exclusiones
importantes:

```rust
// src/lib.rs

// Zona 1 — Lógica de negocio crítica
// Estos módulos manejan dinero y estado de inventario.
// Cobertura target: 90%+ líneas, 80%+ branches
pub mod pricing;      // cálculo de precios, descuentos, impuestos
pub mod inventory;    // control de stock, reservas, alertas
pub mod payments;     // cobros, reembolsos, validaciones
pub mod orders;       // creación, estados, transiciones

// Zona 2 — Soporte
// Estos módulos facilitan operaciones pero no contienen
// lógica de negocio directa. Cobertura target: 60-80%
pub mod config;       // lectura de configuración
pub mod notifications;// envío de emails/push (se testea con fakes)
pub mod reporting;    // generación de reportes

// Zona 3 — Excluidos de métricas
// Documentar razón de exclusión para cada módulo
#[cfg(not(tarpaulin_include))]
pub mod cli;          // Razón: output formatting, testeado manualmente
```

---

## 19. Coverage por módulo: la métrica que importa

La cobertura global es un promedio que oculta los problemas reales. Lo que importa
es la cobertura **por módulo**, especialmente de los módulos críticos.

### Obtener cobertura por módulo con tarpaulin

```bash
# Mostrar cobertura por archivo
cargo tarpaulin --out json 2>/dev/null | \
    jq -r '.files[] | "\(.path): \(.covered)/\(.coverable) = \(if .coverable > 0 then (.covered/.coverable*100 | round) else 0 end)%"' | \
    sort
```

Salida típica:

```
src/config.rs: 15/22 = 68%
src/inventory.rs: 85/92 = 92%
src/main.rs: 0/8 = 0%            ← excluir
src/notifications.rs: 12/20 = 60%
src/orders.rs: 110/125 = 88%
src/payments.rs: 95/98 = 96%
src/pricing.rs: 78/80 = 97%
src/reporting.rs: 5/30 = 16%     ← ¿es Zona 2 o 3?
```

### Obtener cobertura por módulo con llvm-cov

```bash
# Reporte tabular por archivo
cargo llvm-cov --ignore-filename-regex 'main\.rs' report 2>/dev/null
```

Salida:

```
Filename                Regions  Miss  Cover  Lines  Miss  Cover  Branches  Miss  Cover
--------------------------------------------------------------------------------------------
src/config.rs                12     4  66.67%    22     7  68.18%         8     3  62.50%
src/inventory.rs             45     3  93.33%    92     7  92.39%        28     5  82.14%
src/notifications.rs         10     4  60.00%    20     8  60.00%         6     3  50.00%
src/orders.rs                62     8  87.10%   125    15  88.00%        36     8  77.78%
src/payments.rs              50     2  96.00%    98     3  96.94%        30     2  93.33%
src/pricing.rs               38     1  97.37%    80     2  97.50%        24     1  95.83%
src/reporting.rs             15    12  20.00%    30    25  16.67%         8     7  12.50%
```

### Interpretación

```
┌───────────────────────────────────────────────────────────────┐
│              Análisis de cobertura por módulo                  │
│                                                               │
│  pricing.rs    ████████████████████████████████████████ 97%   │
│  payments.rs   ████████████████████████████████████████ 96%   │
│  inventory.rs  ██████████████████████████████████████░░ 92%   │
│  orders.rs     ████████████████████████████████████░░░░ 88%   │
│  config.rs     ██████████████████████████░░░░░░░░░░░░░░ 68%   │
│  notify.rs     ████████████████████████░░░░░░░░░░░░░░░░ 60%   │
│  reporting.rs  ██████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 16%   │
│                                                               │
│  ✓ Zona 1 (pricing, payments, inventory, orders): OK          │
│    Todos > 85%, payments y pricing > 95%                      │
│                                                               │
│  ✓ Zona 2 (config, notifications): Aceptable                 │
│    config 68% — suficiente para código de soporte              │
│    notifications 60% — usa fakes, OK                          │
│                                                               │
│  ⚠ reporting.rs: 16% — ¿debería ser Zona 2 o Zona 3?         │
│    Si es Zona 2: necesita más tests                           │
│    Si es Zona 3: excluir de métricas                          │
│                                                               │
│  Cobertura global: 82% (con main excluido)                    │
│  Cobertura Zona 1: 93% ← ESTA es la métrica que importa      │
└───────────────────────────────────────────────────────────────┘
```

---

## 20. Umbrales diferenciados por zona

### Tabla de umbrales recomendados

| Zona | Tipo proyecto | Line coverage | Branch coverage | Acción si falla |
|---|---|---|---|---|
| 1 | Fintech | >= 95% | >= 85% | Bloquear PR |
| 1 | SaaS general | >= 85% | >= 70% | Bloquear PR |
| 1 | CLI tool | >= 80% | >= 60% | Warning |
| 2 | Cualquiera | >= 60% | >= 40% | Warning |
| 3 | Cualquiera | Excluido | Excluido | N/A |

### Implementar umbrales diferenciados en CI

Ni `cargo-tarpaulin` ni `cargo-llvm-cov` soportan umbrales por directorio de forma
nativa. Pero puedes implementarlo con un script:

```bash
#!/bin/bash
# scripts/check_coverage.sh
# Verifica cobertura por zona con umbrales diferenciados

set -euo pipefail

# Generar reporte JSON
cargo llvm-cov \
    --ignore-filename-regex 'main\.rs$|bin/' \
    --json \
    --output-path target/coverage.json

# Definir zonas y umbrales
declare -A ZONE1_FILES=(
    ["src/pricing.rs"]=90
    ["src/payments.rs"]=90
    ["src/inventory.rs"]=85
    ["src/orders.rs"]=85
)

declare -A ZONE2_FILES=(
    ["src/config.rs"]=60
    ["src/notifications.rs"]=60
)

FAILED=0

check_file() {
    local file=$1
    local threshold=$2
    local zone=$3

    local coverage
    coverage=$(jq -r \
        --arg file "$file" \
        '.data[0].files[] | select(.filename | endswith($file)) | 
         if .summary.lines.count > 0 then
             (.summary.lines.covered / .summary.lines.count * 100 | round)
         else 0 end' \
        target/coverage.json)

    if [ -z "$coverage" ]; then
        echo "  ⚠ $file: not found in coverage report"
        return
    fi

    if [ "$coverage" -lt "$threshold" ]; then
        echo "  ✗ $file: ${coverage}% < ${threshold}% (Zone $zone)"
        FAILED=1
    else
        echo "  ✓ $file: ${coverage}% >= ${threshold}% (Zone $zone)"
    fi
}

echo "=== Zone 1: Critical (high thresholds) ==="
for file in "${!ZONE1_FILES[@]}"; do
    check_file "$file" "${ZONE1_FILES[$file]}" "1"
done

echo ""
echo "=== Zone 2: Support (moderate thresholds) ==="
for file in "${!ZONE2_FILES[@]}"; do
    check_file "$file" "${ZONE2_FILES[$file]}" "2"
done

echo ""
if [ "$FAILED" -eq 1 ]; then
    echo "❌ Coverage check FAILED"
    exit 1
else
    echo "✅ Coverage check PASSED"
fi
```

### Uso en GitHub Actions

```yaml
- name: Check coverage by zone
  run: bash scripts/check_coverage.sh
```

---

## 21. El anti-patrón del test cosmético

El "test cosmético" es un test que existe solo para subir el número de cobertura sin
verificar nada útil:

### Ejemplos de tests cosméticos

```rust
// ❌ Anti-patrón: test que solo llama a la función
#[test]
fn test_calculate_tax() {
    let _ = calculate_tax(100.0, "US-CA");
    // Sin assert! — cubre las líneas pero no verifica nada
}

// ❌ Anti-patrón: test que verifica lo obvio
#[test]
fn test_new_user() {
    let user = User::new("Alice".to_string(), "alice@example.com".to_string());
    assert!(user.name.len() > 0);  // ← siempre true si le pasamos "Alice"
}

// ❌ Anti-patrón: test que solo verifica que no hay panic
#[test]
fn test_process_order() {
    let order = Order::sample();
    let result = process_order(order);
    assert!(result.is_ok());  // ← no verifica QUÉ contiene el Ok
}

// ❌ Anti-patrón: test que verifica el formato del string
#[test]
fn test_error_display() {
    let err = AppError::NotFound("user-42".to_string());
    let msg = err.to_string();
    assert!(!msg.is_empty());  // ← siempre true
}
```

### Cómo convertir un test cosmético en un test útil

```rust
// ✓ Test real: verifica el resultado del cálculo
#[test]
fn california_tax_is_7_25_percent() {
    let tax = calculate_tax(100.0, "US-CA");
    assert!((tax - 7.25).abs() < f64::EPSILON);
}

// ✓ Test real: verifica invariantes del constructor
#[test]
fn new_user_has_default_role_and_creation_date() {
    let user = User::new("Alice".to_string(), "alice@example.com".to_string());
    assert_eq!(user.role, Role::Standard);
    assert!(user.created_at <= SystemTime::now());
}

// ✓ Test real: verifica el contenido del resultado
#[test]
fn process_order_returns_receipt_with_correct_total() {
    let order = Order {
        items: vec![
            Item { name: "Widget".into(), price: 10.0, qty: 3 },
        ],
        discount: None,
    };
    let receipt = process_order(order).unwrap();
    assert_eq!(receipt.total, 30.0);
    assert_eq!(receipt.items_count, 1);
}
```

### Detector de tests cosméticos

Una heurística simple: si puedes eliminar todos los `assert!` de un test y sigue
pasando, el test no verifica nada. La herramienta `cargo-mutants` automatiza esto
(ver sección 23).

```
┌──────────────────────────────────────────────────────────────┐
│          Señales de test cosmético                            │
│                                                              │
│  1. No tiene assert! (o solo assert!(result.is_ok()))        │
│  2. Solo verifica que no hay panic                           │
│  3. Los asserts son tautológicos (assert!(true))             │
│  4. Verifica propiedades triviales (len > 0, !is_empty)      │
│  5. Fue escrito después de medir cobertura (no antes)        │
│  6. El nombre es genérico: "test_function_name"              │
│  7. No tiene caso borde ni input interesante                 │
│  8. Si cambias la lógica de la función, el test sigue        │
│     pasando                                                  │
└──────────────────────────────────────────────────────────────┘
```

---

## 22. Mutation testing: cuando la cobertura miente

La cobertura de líneas responde: "¿se ejecutó esta línea?"
La mutation testing responde: "¿los tests detectarían si esta línea fuera incorrecta?"

### Concepto

```
┌──────────────────────────────────────────────────────────────────────┐
│              Mutation testing                                        │
│                                                                      │
│  Código original:                                                    │
│  fn discount(price: f64, pct: f64) -> f64 {                         │
│      price * (1.0 - pct)                                             │
│  }                                                                   │
│                                                                      │
│  Mutante 1: cambiar - por +                                         │
│  fn discount(price: f64, pct: f64) -> f64 {                         │
│      price * (1.0 + pct)   // ← mutación                            │
│  }                                                                   │
│                                                                      │
│  Mutante 2: cambiar * por /                                         │
│  fn discount(price: f64, pct: f64) -> f64 {                         │
│      price / (1.0 - pct)   // ← mutación                            │
│  }                                                                   │
│                                                                      │
│  Mutante 3: retornar 0.0                                            │
│  fn discount(price: f64, pct: f64) -> f64 {                         │
│      0.0                    // ← mutación                            │
│  }                                                                   │
│                                                                      │
│  Para cada mutante, se ejecutan todos los tests:                     │
│  • Si algún test FALLA → mutante "killed" ✓ (tests son buenos)      │
│  • Si todos PASAN → mutante "survived" ✗ (tests son débiles)        │
│                                                                      │
│  Mutation score = killed / total                                     │
│  Target: > 80% para código crítico                                  │
└──────────────────────────────────────────────────────────────────────┘
```

### Tipos de mutaciones

| Mutación | Original | Mutado | Qué revela si sobrevive |
|---|---|---|---|
| Aritmética | `a + b` | `a - b`, `a * b` | Tests no verifican el resultado numérico |
| Relacional | `a < b` | `a <= b`, `a > b` | Tests no cubren el límite |
| Lógica | `a && b` | `a \|\| b` | Tests no verifican ambas condiciones |
| Negación | `!flag` | `flag` | Tests no verifican el caso negativo |
| Retorno | `return x` | `return Default::default()` | Tests no verifican el valor retornado |
| Eliminación | `statement;` | `// removed` | Tests no dependen de ese side effect |
| Constante | `42` | `0`, `1`, `-1` | Tests no verifican el valor exacto |
| Boundary | `a < 10` | `a < 9`, `a < 11` | Tests no prueban el límite exacto |

### Ejemplo: mutation testing revela test cosmético

```rust
fn classify_age(age: u32) -> &'static str {
    if age < 13 {
        "child"
    } else if age < 18 {
        "teenager"
    } else {
        "adult"
    }
}

// Test cosmético que da 100% cobertura
#[test]
fn test_classify_age() {
    assert_eq!(classify_age(5), "child");
    assert_eq!(classify_age(15), "teenager");
    assert_eq!(classify_age(25), "adult");
}
```

Cobertura: 100% de líneas. Pero mutation testing revela:

```
Mutante: age < 13 → age < 12
  Resultado: tests pasan → SURVIVED ✗
  Problema: no testeamos age=12

Mutante: age < 13 → age <= 13
  Resultado: tests pasan → SURVIVED ✗
  Problema: no testeamos age=13

Mutante: age < 18 → age < 17
  Resultado: tests pasan → SURVIVED ✗
  Problema: no testeamos age=17
```

Tests mejorados tras mutation testing:

```rust
#[test]
fn age_boundaries() {
    // Exactamente en los límites
    assert_eq!(classify_age(12), "child");     // último child
    assert_eq!(classify_age(13), "teenager");  // primer teenager
    assert_eq!(classify_age(17), "teenager");  // último teenager
    assert_eq!(classify_age(18), "adult");     // primer adult
}

#[test]
fn age_extremes() {
    assert_eq!(classify_age(0), "child");
    assert_eq!(classify_age(u32::MAX), "adult");
}
```

Ahora todos los mutantes son killed.

---

## 23. cargo-mutants: verificar que los tests detectan bugs

`cargo-mutants` es la herramienta de mutation testing más usada en el ecosistema Rust.

### Instalación

```bash
cargo install cargo-mutants
```

### Uso básico

```bash
# Ejecutar mutation testing en todo el proyecto
cargo mutants

# Solo en un archivo específico
cargo mutants -- --file src/pricing.rs

# Solo en una función específica
cargo mutants -- --re "calculate_discount"

# Con paralelismo
cargo mutants -j 4
```

### Output típico

```
Found 47 mutants to test
 47/47 mutants tested in 2m 30s

 37 mutants killed (tests caught the bug)
  7 mutants survived (tests did NOT catch the bug)
  3 mutants timed out (possible infinite loop)

Survived mutants:
  src/pricing.rs:42: replace * with / in calculate_base_price
  src/pricing.rs:58: replace > with >= in is_eligible_for_discount
  src/pricing.rs:73: replace 0.25 with 1.0 in max_discount_cap
  src/inventory.rs:15: replace - with + in decrease_stock
  src/inventory.rs:28: delete call to notify_low_stock
  src/orders.rs:91: replace == with != in is_valid_transition
  src/orders.rs:105: return Default::default() in calculate_shipping

Mutation score: 37/47 = 78.7%
```

### Interpretar resultados

```
┌──────────────────────────────────────────────────────────────────────┐
│         Interpretar resultados de mutation testing                    │
│                                                                      │
│  KILLED (37/47) ✓                                                    │
│  Los tests detectaron la mutación. Bien.                             │
│                                                                      │
│  SURVIVED (7/47) ✗                                                   │
│  Los tests NO detectaron la mutación. Dos posibilidades:             │
│                                                                      │
│  1. Test faltante: la mutación cambia comportamiento real             │
│     pero ningún test lo verifica.                                    │
│     ACCIÓN: escribir un test que cubra ese caso                      │
│                                                                      │
│  2. Mutación equivalente: la mutación no cambia el                   │
│     comportamiento observable (ej: cambiar el orden de                │
│     operaciones conmutativas).                                       │
│     ACCIÓN: ignorar (cargo-mutants no puede distinguir esto)         │
│                                                                      │
│  TIMEOUT (3/47)                                                      │
│  La mutación causó un loop infinito. Generalmente esto               │
│  significa que los tests SÍ detectarían el problema (el              │
│  programa se cuelga), así que se cuenta como "killed".               │
│                                                                      │
│  Score objetivo:                                                     │
│  • Zona 1: > 80% mutation score                                     │
│  • Zona 2: > 60% mutation score                                     │
│  • Global: > 70% mutation score                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Mutation testing en CI

```yaml
# Solo en módulos críticos para no hacer el CI lento
- name: Mutation testing (critical modules)
  run: |
    cargo mutants \
      -- --file src/pricing.rs --file src/payments.rs \
      -j 4 \
      --timeout 60
```

**Nota importante**: mutation testing es lento (ejecuta toda la suite de tests por
cada mutante). No lo uses en CI para todo el proyecto — solo para módulos Zona 1.

### Comparación con C y Go

| Aspecto | C | Go | Rust |
|---|---|---|---|
| Herramienta | Mull (LLVM-based) | go-mutesting | cargo-mutants |
| Instalación | Compilar desde source | `go install` | `cargo install` |
| Integración | Limitada | Básica | Buena (Cargo nativo) |
| Velocidad | Lenta | Media | Media-lenta |
| Madurez | Experimental | Estable | Activa development |
| Mutaciones | Aritméticas, lógicas | Expresiones | Todas las anteriores + return |

---

## 24. Comparación con C y Go

### Estrategias de cobertura realista por lenguaje

| Aspecto | C | Go | Rust |
|---|---|---|---|
| **Zona 3 típica** | `main()`, `printf` wrappers, signal handlers | `main()`, `log.Fatal`, HTTP boilerplate | `main()`, `Display` triviales, `From` impls |
| **Mecanismo de exclusión** | `// LCOV_EXCL_LINE`, `GCOV_EXCL_START/STOP` | `//go:build ignore`, `-coverprofile` + filtro | `#[tarpaulin::skip]`, `--ignore-filename-regex` |
| **Thin main pattern** | `app_run(argc, argv)` en `app.c` | `func run(args []string) error` en `run.go` | `pub fn run() -> Result<>` en `lib.rs` |
| **Error path testing** | Todos manuales (no hay `?`) | `if err != nil` es repetitivo pero testeable | `?` mecánico (no testear), `match` con lógica (sí testear) |
| **Display/formatting** | `printf` no genera cobertura propia | `String()` methods — generalmente no testear | `Display` impls — generalmente no testear |
| **Mutation testing** | Mull (LLVM), limitado | go-mutesting | cargo-mutants |
| **Cobertura de branch** | gcov `--branch-probabilities` | Limitada (solo statement) | llvm-cov nativo |
| **Métrica principal** | Line + branch (gcov) | Statement (go tool cover) | Line + branch (llvm-cov) |
| **Fake implementations** | Structs con function pointers | Interfaces + in-memory impls | Traits + in-memory impls |
| **Contract testing** | Macros que testean interface compliance | Subtests con interface parameter | Funciones genéricas `<T: Trait>` |

### Exclusiones idiomáticas por lenguaje

**C — gcov/lcov**:

```c
/* LCOV_EXCL_START */
void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "  -v  verbose\n");
    fprintf(stderr, "  -h  help\n");
}
/* LCOV_EXCL_STOP */

int main(int argc, char *argv[]) {  /* LCOV_EXCL_LINE */
    return app_run(argc, argv);     /* LCOV_EXCL_LINE */
}
```

**Go — go test -coverprofile**:

```go
// No hay mecanismo de exclusión en código en Go.
// Se filtra el coverage profile post-generación:
// grep -v 'main.go\|generated' cover.out > cover_filtered.out

// La convención es separar el código no testeable:
// main.go       → solo main(), no se mide
// run.go        → lógica real, se mide
// run_test.go   → tests de run.go
```

**Rust — tarpaulin + llvm-cov**:

```rust
// Tarpaulin: atributo en código
#[tarpaulin::skip]
fn main() { /* ... */ }

// llvm-cov: filtro por archivo
// cargo llvm-cov --ignore-filename-regex 'main\.rs'

// Combinación: usar ambos
#[cfg(not(tarpaulin_include))]  // para tarpaulin
fn main() {                      // llvm-cov excluye main.rs completo
    if let Err(e) = run() {
        eprintln!("{e}");
        std::process::exit(1);
    }
}
```

---

## 25. Errores comunes

### Error 1: perseguir el 100% a toda costa

```
❌ Problema:
  El equipo decide que todo PR debe mantener 100% de cobertura.
  Los desarrolladores escriben tests cosméticos para líneas triviales.
  El tiempo de CI se triplica. La confianza real no aumenta.

✓ Solución:
  Establecer umbrales por zona:
  - Zona 1: 85-95%
  - Zona 2: 60-80%
  - Zona 3: excluir
  Medir mutation score para Zona 1 en lugar de solo cobertura.
```

### Error 2: no excluir nada y tener "cobertura baja"

```
❌ Problema:
  El proyecto reporta 55% de cobertura. El manager está preocupado.
  Pero el 30% del código es main(), Display impls, y código generado.
  La lógica de negocio en realidad tiene 85%.

✓ Solución:
  Excluir Zona 3 de métricas.
  Reportar cobertura de Zona 1 por separado.
  La cobertura global sube a 78% "gratis" (reflejando la realidad).
```

### Error 3: usar cobertura global como gate de CI sin matices

```
❌ Problema:
  CI bloquea PRs con cobertura global < 80%.
  Un PR que agrega un nuevo módulo CLI (Zona 3) baja la cobertura
  de 82% a 76% sin tocar lógica de negocio.
  El desarrollador tiene que escribir tests inútiles para el CLI.

✓ Solución:
  Usar --ignore-filename-regex para excluir Zona 3.
  O usar el script de umbrales por zona (sección 20).
  O usar patch coverage: medir solo las líneas nuevas/modificadas.
```

### Error 4: medir cobertura sin mutation testing

```
❌ Problema:
  Proyecto con 90% de cobertura. Se siente bien.
  cargo-mutants revela que el mutation score es 45%.
  La mitad de los tests no verifican nada significativo.

✓ Solución:
  Complementar cobertura con cargo-mutants en Zona 1.
  Revisar tests que no tienen asserts significativos.
  Priorizar la calidad de los tests sobre la cantidad.
```

### Error 5: excluir código que SÍ debería testearse

```
❌ Problema:
  El desarrollador marca una función como #[tarpaulin::skip]
  porque "es difícil de testear". Pero la función contiene
  lógica de validación de pagos.

✓ Solución:
  Regla: solo excluir código que es mecánico o cosmético.
  Si la función tiene if/else/match con lógica de decisión,
  NO excluir — refactorizar para hacerla testeable.
  Code review de exclusiones: cada #[tarpaulin::skip] requiere
  justificación en el PR.
```

### Error 6: no revisar la cobertura de branch en Zona 1

```
❌ Problema:
  Función con 100% de line coverage pero 50% de branch coverage.
  fn apply_discount(price: f64, is_vip: bool) -> f64 {
      if is_vip { price * 0.8 } else { price }  // una línea, dos branches
  }
  Solo se testea con is_vip = true.

✓ Solución:
  Para Zona 1, exigir branch coverage además de line coverage.
  cargo llvm-cov --fail-under-branches 70
  Revisar el HTML report buscando branches no cubiertos.
```

### Error 7: contar líneas de test como cobertura

```
❌ Problema:
  El reporte incluye los archivos de test en la cobertura.
  Los tests siempre se "cubren" a sí mismos (se ejecutan).
  Esto infla la cobertura artificialmente.

✓ Solución:
  cargo tarpaulin excluye tests por defecto (--run-types Tests).
  cargo llvm-cov excluye tests/ por defecto.
  Verificar que el reporte no incluya archivos _test.rs o tests/.
```

---

## 26. Ejemplo completo: sistema de facturación

Construyamos un sistema que demuestre las tres zonas en acción.

### Estructura del proyecto

```
invoice_system/
├── Cargo.toml
├── .tarpaulin.toml
├── src/
│   ├── main.rs           ← ZONA 3: thin main
│   ├── lib.rs             ← organización de módulos
│   ├── invoice.rs         ← ZONA 1: lógica de facturación
│   ├── tax.rs             ← ZONA 1: cálculo de impuestos
│   ├── discount.rs        ← ZONA 1: reglas de descuento
│   ├── customer.rs        ← ZONA 2: modelo de cliente
│   ├── config.rs          ← ZONA 2: configuración
│   ├── error.rs           ← ZONA 3: Display/From triviales
│   └── report.rs          ← ZONA 3: formateo de reportes
```

### Cargo.toml

```toml
[package]
name = "invoice_system"
version = "0.1.0"
edition = "2021"

[dependencies]
chrono = "0.4"
thiserror = "2"

[dev-dependencies]
```

### .tarpaulin.toml

```toml
[default]
exclude-files = [
    "src/main.rs",
    "src/report.rs",
]
out = ["html", "lcov"]
engine = "llvm"

[ci]
inherits = "default"
fail-under = 75
out = ["lcov"]
```

### src/main.rs — ZONA 3

```rust
//! Punto de entrada — thin main, excluido de cobertura.

#[cfg(not(tarpaulin_include))]
fn main() {
    if let Err(e) = invoice_system::run() {
        eprintln!("Error: {e}");
        std::process::exit(1);
    }
}
```

### src/error.rs — ZONA 3

```rust
//! Tipos de error — Display/From mecánicos, excluidos de cobertura.

use thiserror::Error;

#[derive(Debug, Error)]
pub enum AppError {
    #[error("Invalid price: {0}")]
    InvalidPrice(f64),

    #[error("Invalid quantity: {0}")]
    InvalidQuantity(u32),

    #[error("Unknown tax region: {0}")]
    UnknownRegion(String),

    #[error("Discount exceeds maximum: {applied}% > {max}%")]
    DiscountExceeded { applied: f64, max: f64 },

    #[error("Customer not found: {0}")]
    CustomerNotFound(String),

    #[error("Config error: {0}")]
    Config(String),

    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
}

// thiserror genera Display y From automáticamente.
// No hay lógica de decisión — todo es mecánico.
// Excluir de métricas de cobertura.
```

### src/lib.rs

```rust
pub mod config;
pub mod customer;
pub mod discount;
pub mod error;
pub mod invoice;
pub mod tax;

#[cfg(not(tarpaulin_include))]
pub mod report;

use error::AppError;

pub fn run() -> Result<(), AppError> {
    let config = config::AppConfig::default();
    let customer = customer::Customer::new(
        "CUST-001".to_string(),
        "Acme Corp".to_string(),
        customer::Tier::Gold,
        "US-CA".to_string(),
    );

    let items = vec![
        invoice::LineItem::new("Widget A".to_string(), 25.50, 10)?,
        invoice::LineItem::new("Widget B".to_string(), 99.99, 2)?,
    ];

    let inv = invoice::Invoice::create(
        customer,
        items,
        &config,
    )?;

    println!("{}", report::format_invoice(&inv));
    Ok(())
}
```

### src/tax.rs — ZONA 1

```rust
//! Cálculo de impuestos por región.
//! ZONA 1: cada error aquí tiene consecuencias legales.

use crate::error::AppError;

/// Tabla de tasas de impuesto por región.
/// En producción esto vendría de una base de datos.
pub fn tax_rate(region: &str) -> Result<f64, AppError> {
    match region {
        "US-CA" => Ok(0.0725),  // California 7.25%
        "US-NY" => Ok(0.08),    // New York 8%
        "US-TX" => Ok(0.0625),  // Texas 6.25%
        "US-OR" => Ok(0.0),     // Oregon: no sales tax
        "US-DE" => Ok(0.0),     // Delaware: no sales tax
        "EU-DE" => Ok(0.19),    // Germany 19%
        "EU-FR" => Ok(0.20),    // France 20%
        "EU-ES" => Ok(0.21),    // Spain 21%
        "MX"    => Ok(0.16),    // Mexico 16%
        other   => Err(AppError::UnknownRegion(other.to_string())),
    }
}

/// Calcula el impuesto sobre un monto base.
/// Reglas:
/// - El impuesto se redondea a 2 decimales
/// - Montos negativos son rechazados (el llamador debe validar)
pub fn calculate_tax(base_amount: f64, region: &str) -> Result<f64, AppError> {
    if base_amount < 0.0 {
        return Err(AppError::InvalidPrice(base_amount));
    }
    let rate = tax_rate(region)?;
    let tax = base_amount * rate;
    // Redondear a 2 decimales (centavos)
    Ok((tax * 100.0).round() / 100.0)
}

#[cfg(test)]
mod tests {
    use super::*;

    // --- tax_rate: verificar cada región ---

    #[test]
    fn california_rate() {
        assert_eq!(tax_rate("US-CA").unwrap(), 0.0725);
    }

    #[test]
    fn oregon_is_zero() {
        assert_eq!(tax_rate("US-OR").unwrap(), 0.0);
    }

    #[test]
    fn unknown_region_is_error() {
        let err = tax_rate("MARS").unwrap_err();
        assert!(matches!(err, AppError::UnknownRegion(r) if r == "MARS"));
    }

    // --- calculate_tax: verificar cálculos ---

    #[test]
    fn tax_on_100_in_california() {
        let tax = calculate_tax(100.0, "US-CA").unwrap();
        assert_eq!(tax, 7.25);
    }

    #[test]
    fn tax_rounds_to_two_decimals() {
        // 33.33 * 0.0725 = 2.416425 → redondeado a 2.42
        let tax = calculate_tax(33.33, "US-CA").unwrap();
        assert_eq!(tax, 2.42);
    }

    #[test]
    fn tax_on_zero_is_zero() {
        let tax = calculate_tax(0.0, "US-CA").unwrap();
        assert_eq!(tax, 0.0);
    }

    #[test]
    fn tax_in_tax_free_region_is_zero() {
        let tax = calculate_tax(1000.0, "US-OR").unwrap();
        assert_eq!(tax, 0.0);
    }

    #[test]
    fn negative_amount_is_error() {
        let err = calculate_tax(-1.0, "US-CA").unwrap_err();
        assert!(matches!(err, AppError::InvalidPrice(p) if p == -1.0));
    }

    #[test]
    fn unknown_region_propagates() {
        let err = calculate_tax(100.0, "MOON").unwrap_err();
        assert!(matches!(err, AppError::UnknownRegion(_)));
    }

    #[test]
    fn spain_21_percent() {
        let tax = calculate_tax(100.0, "EU-ES").unwrap();
        assert_eq!(tax, 21.0);
    }

    #[test]
    fn mexico_16_percent() {
        let tax = calculate_tax(200.0, "MX").unwrap();
        assert_eq!(tax, 32.0);
    }
}
```

### src/discount.rs — ZONA 1

```rust
//! Motor de descuentos.
//! ZONA 1: errores aquí = cobrar de más o de menos al cliente.

use crate::customer::Tier;
use crate::error::AppError;

/// Porcentaje de descuento por tier del cliente.
pub fn tier_discount(tier: &Tier) -> f64 {
    match tier {
        Tier::Standard => 0.0,
        Tier::Silver   => 0.03,
        Tier::Gold     => 0.07,
        Tier::Platinum => 0.12,
    }
}

/// Descuento por volumen basado en cantidad total de items.
pub fn volume_discount(total_items: u32) -> f64 {
    match total_items {
        0..=9   => 0.0,
        10..=49 => 0.02,
        50..=99 => 0.05,
        _       => 0.08,
    }
}

/// Calcula el descuento final combinando tier y volumen.
/// Reglas de negocio:
/// 1. Se suman tier + volumen
/// 2. El descuento total no puede exceder MAX_DISCOUNT (20%)
/// 3. El descuento se aplica sobre el subtotal antes de impuestos
pub const MAX_DISCOUNT: f64 = 0.20;

pub fn calculate_discount(
    subtotal: f64,
    tier: &Tier,
    total_items: u32,
) -> Result<DiscountResult, AppError> {
    if subtotal < 0.0 {
        return Err(AppError::InvalidPrice(subtotal));
    }

    let tier_pct = tier_discount(tier);
    let volume_pct = volume_discount(total_items);
    let combined_pct = tier_pct + volume_pct;

    if combined_pct > MAX_DISCOUNT {
        // No es un error — solo capeamos
        let capped = MAX_DISCOUNT;
        return Ok(DiscountResult {
            tier_pct,
            volume_pct,
            applied_pct: capped,
            was_capped: true,
            amount: (subtotal * capped * 100.0).round() / 100.0,
        });
    }

    Ok(DiscountResult {
        tier_pct,
        volume_pct,
        applied_pct: combined_pct,
        was_capped: false,
        amount: (subtotal * combined_pct * 100.0).round() / 100.0,
    })
}

#[derive(Debug, PartialEq)]
pub struct DiscountResult {
    pub tier_pct: f64,
    pub volume_pct: f64,
    pub applied_pct: f64,
    pub was_capped: bool,
    pub amount: f64,
}

#[cfg(test)]
mod tests {
    use super::*;

    // --- tier_discount ---

    #[test]
    fn standard_no_discount() {
        assert_eq!(tier_discount(&Tier::Standard), 0.0);
    }

    #[test]
    fn platinum_12_percent() {
        assert_eq!(tier_discount(&Tier::Platinum), 0.12);
    }

    // --- volume_discount ---

    #[test]
    fn under_10_items_no_volume_discount() {
        assert_eq!(volume_discount(0), 0.0);
        assert_eq!(volume_discount(9), 0.0);
    }

    #[test]
    fn boundary_10_items_gets_2_percent() {
        assert_eq!(volume_discount(10), 0.02);
    }

    #[test]
    fn boundary_50_items_gets_5_percent() {
        assert_eq!(volume_discount(50), 0.05);
    }

    #[test]
    fn over_100_items_gets_8_percent() {
        assert_eq!(volume_discount(100), 0.08);
        assert_eq!(volume_discount(1000), 0.08);
    }

    // --- calculate_discount ---

    #[test]
    fn standard_with_few_items_is_zero() {
        let result = calculate_discount(100.0, &Tier::Standard, 5).unwrap();
        assert_eq!(result.applied_pct, 0.0);
        assert_eq!(result.amount, 0.0);
        assert!(!result.was_capped);
    }

    #[test]
    fn gold_with_50_items_combines_discounts() {
        // Gold = 7%, 50 items = 5%, combined = 12%
        let result = calculate_discount(1000.0, &Tier::Gold, 50).unwrap();
        assert_eq!(result.tier_pct, 0.07);
        assert_eq!(result.volume_pct, 0.05);
        assert_eq!(result.applied_pct, 0.12);
        assert_eq!(result.amount, 120.0);
        assert!(!result.was_capped);
    }

    #[test]
    fn platinum_with_100_items_gets_capped() {
        // Platinum = 12%, 100 items = 8%, combined = 20% = MAX
        let result = calculate_discount(1000.0, &Tier::Platinum, 100).unwrap();
        assert_eq!(result.applied_pct, 0.20);
        assert!(result.was_capped);
        assert_eq!(result.amount, 200.0);
    }

    #[test]
    fn platinum_with_50_items_not_capped() {
        // Platinum = 12%, 50 items = 5%, combined = 17% < 20%
        let result = calculate_discount(1000.0, &Tier::Platinum, 50).unwrap();
        assert_eq!(result.applied_pct, 0.17);
        assert!(!result.was_capped);
    }

    #[test]
    fn discount_rounds_to_cents() {
        // 33.33 * 0.07 = 2.3331 → 2.33
        let result = calculate_discount(33.33, &Tier::Gold, 1).unwrap();
        assert_eq!(result.amount, 2.33);
    }

    #[test]
    fn negative_subtotal_is_error() {
        let err = calculate_discount(-100.0, &Tier::Gold, 10).unwrap_err();
        assert!(matches!(err, AppError::InvalidPrice(_)));
    }
}
```

### src/invoice.rs — ZONA 1

```rust
//! Creación y cálculo de facturas.
//! ZONA 1: la factura es el artefacto central del sistema.

use crate::config::AppConfig;
use crate::customer::Customer;
use crate::discount;
use crate::error::AppError;
use crate::tax;

#[derive(Debug)]
pub struct LineItem {
    pub description: String,
    pub unit_price: f64,
    pub quantity: u32,
    pub line_total: f64,
}

impl LineItem {
    pub fn new(description: String, unit_price: f64, quantity: u32) -> Result<Self, AppError> {
        if unit_price <= 0.0 {
            return Err(AppError::InvalidPrice(unit_price));
        }
        if quantity == 0 {
            return Err(AppError::InvalidQuantity(quantity));
        }
        let line_total = (unit_price * quantity as f64 * 100.0).round() / 100.0;
        Ok(Self {
            description,
            unit_price,
            quantity,
            line_total,
        })
    }
}

#[derive(Debug)]
pub struct Invoice {
    pub customer: Customer,
    pub items: Vec<LineItem>,
    pub subtotal: f64,
    pub discount: discount::DiscountResult,
    pub subtotal_after_discount: f64,
    pub tax_amount: f64,
    pub tax_rate: f64,
    pub total: f64,
}

impl Invoice {
    pub fn create(
        customer: Customer,
        items: Vec<LineItem>,
        _config: &AppConfig,
    ) -> Result<Self, AppError> {
        if items.is_empty() {
            return Err(AppError::InvalidQuantity(0));
        }

        let subtotal: f64 = items.iter().map(|i| i.line_total).sum();
        let total_items: u32 = items.iter().map(|i| i.quantity).sum();

        let disc = discount::calculate_discount(subtotal, &customer.tier, total_items)?;
        let subtotal_after_discount = subtotal - disc.amount;

        let rate = tax::tax_rate(&customer.tax_region)?;
        let tax_amount = tax::calculate_tax(subtotal_after_discount, &customer.tax_region)?;

        let total = (subtotal_after_discount + tax_amount) * 100.0;
        let total = total.round() / 100.0;

        Ok(Invoice {
            customer,
            items,
            subtotal,
            discount: disc,
            subtotal_after_discount,
            tax_amount,
            tax_rate: rate,
            total,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::customer::Tier;

    fn test_config() -> AppConfig {
        AppConfig::default()
    }

    fn test_customer(tier: Tier, region: &str) -> Customer {
        Customer::new(
            "TEST-001".to_string(),
            "Test Corp".to_string(),
            tier,
            region.to_string(),
        )
    }

    // --- LineItem ---

    #[test]
    fn line_item_calculates_total() {
        let item = LineItem::new("Widget".to_string(), 10.50, 3).unwrap();
        assert_eq!(item.line_total, 31.50);
    }

    #[test]
    fn line_item_rounds_total() {
        // 3.33 * 3 = 9.99 exacto, pero 3.33 * 7 = 23.31
        let item = LineItem::new("Widget".to_string(), 3.33, 7).unwrap();
        assert_eq!(item.line_total, 23.31);
    }

    #[test]
    fn line_item_rejects_zero_price() {
        let err = LineItem::new("Widget".to_string(), 0.0, 1).unwrap_err();
        assert!(matches!(err, AppError::InvalidPrice(_)));
    }

    #[test]
    fn line_item_rejects_negative_price() {
        let err = LineItem::new("Widget".to_string(), -5.0, 1).unwrap_err();
        assert!(matches!(err, AppError::InvalidPrice(_)));
    }

    #[test]
    fn line_item_rejects_zero_quantity() {
        let err = LineItem::new("Widget".to_string(), 10.0, 0).unwrap_err();
        assert!(matches!(err, AppError::InvalidQuantity(0)));
    }

    // --- Invoice::create ---

    #[test]
    fn invoice_basic_calculation() {
        let customer = test_customer(Tier::Standard, "US-OR"); // Oregon: no tax
        let items = vec![
            LineItem::new("A".to_string(), 100.0, 1).unwrap(),
        ];
        let inv = Invoice::create(customer, items, &test_config()).unwrap();

        assert_eq!(inv.subtotal, 100.0);
        assert_eq!(inv.discount.amount, 0.0);      // Standard, 1 item
        assert_eq!(inv.tax_amount, 0.0);            // Oregon
        assert_eq!(inv.total, 100.0);
    }

    #[test]
    fn invoice_with_discount_and_tax() {
        let customer = test_customer(Tier::Gold, "US-CA");
        let items = vec![
            LineItem::new("A".to_string(), 100.0, 5).unwrap(),
            LineItem::new("B".to_string(), 50.0, 5).unwrap(),
        ];
        // subtotal = 500 + 250 = 750
        // Gold = 7%, 10 items = 2%, combined = 9%
        // discount = 750 * 0.09 = 67.50
        // after discount = 682.50
        // CA tax = 682.50 * 0.0725 = 49.48
        // total = 682.50 + 49.48 = 731.98

        let inv = Invoice::create(customer, items, &test_config()).unwrap();

        assert_eq!(inv.subtotal, 750.0);
        assert_eq!(inv.discount.applied_pct, 0.09);
        assert_eq!(inv.discount.amount, 67.50);
        assert_eq!(inv.subtotal_after_discount, 682.50);
        assert_eq!(inv.tax_amount, 49.48);
        assert_eq!(inv.total, 731.98);
    }

    #[test]
    fn invoice_with_capped_discount() {
        let customer = test_customer(Tier::Platinum, "US-DE"); // Delaware: no tax
        let items = vec![
            LineItem::new("Bulk".to_string(), 10.0, 100).unwrap(),
        ];
        // subtotal = 1000
        // Platinum = 12%, 100 items = 8%, combined = 20% = MAX
        // discount = 1000 * 0.20 = 200
        // after discount = 800
        // no tax (Delaware)
        // total = 800

        let inv = Invoice::create(customer, items, &test_config()).unwrap();

        assert!(inv.discount.was_capped);
        assert_eq!(inv.discount.applied_pct, 0.20);
        assert_eq!(inv.total, 800.0);
    }

    #[test]
    fn invoice_rejects_empty_items() {
        let customer = test_customer(Tier::Standard, "US-CA");
        let err = Invoice::create(customer, vec![], &test_config()).unwrap_err();
        assert!(matches!(err, AppError::InvalidQuantity(0)));
    }

    #[test]
    fn invoice_propagates_unknown_region() {
        let customer = test_customer(Tier::Standard, "MARS");
        let items = vec![
            LineItem::new("X".to_string(), 10.0, 1).unwrap(),
        ];
        let err = Invoice::create(customer, items, &test_config()).unwrap_err();
        assert!(matches!(err, AppError::UnknownRegion(_)));
    }

    #[test]
    fn invoice_tax_applied_after_discount() {
        // Verificar que el impuesto se calcula sobre el monto
        // DESPUÉS del descuento, no antes
        let customer = test_customer(Tier::Gold, "MX"); // Mexico 16%
        let items = vec![
            LineItem::new("A".to_string(), 1000.0, 1).unwrap(),
        ];
        // subtotal = 1000
        // Gold = 7%, 1 item = 0%, combined = 7%
        // discount = 70
        // after discount = 930
        // MX tax = 930 * 0.16 = 148.80
        // total = 930 + 148.80 = 1078.80

        let inv = Invoice::create(customer, items, &test_config()).unwrap();

        assert_eq!(inv.subtotal_after_discount, 930.0);
        assert_eq!(inv.tax_amount, 148.80);
        assert_eq!(inv.total, 1078.80);
    }
}
```

### src/customer.rs — ZONA 2

```rust
//! Modelo de cliente.
//! ZONA 2: datos de soporte, sin lógica de negocio compleja.

#[derive(Debug, Clone)]
pub struct Customer {
    pub id: String,
    pub name: String,
    pub tier: Tier,
    pub tax_region: String,
}

impl Customer {
    pub fn new(id: String, name: String, tier: Tier, tax_region: String) -> Self {
        Self { id, name, tier, tax_region }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum Tier {
    Standard,
    Silver,
    Gold,
    Platinum,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn customer_creation() {
        let c = Customer::new(
            "C-001".to_string(),
            "Test".to_string(),
            Tier::Gold,
            "US-CA".to_string(),
        );
        assert_eq!(c.id, "C-001");
        assert_eq!(c.tier, Tier::Gold);
    }
}
```

### src/config.rs — ZONA 2

```rust
//! Configuración de la aplicación.
//! ZONA 2: código de soporte.

#[derive(Debug)]
pub struct AppConfig {
    pub company_name: String,
    pub currency: String,
    pub max_items_per_invoice: u32,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            company_name: "My Company".to_string(),
            currency: "USD".to_string(),
            max_items_per_invoice: 100,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_config_has_reasonable_values() {
        let config = AppConfig::default();
        assert!(!config.company_name.is_empty());
        assert_eq!(config.currency, "USD");
        assert!(config.max_items_per_invoice > 0);
    }
}
```

### src/report.rs — ZONA 3

```rust
//! Formateo de reportes para terminal.
//! ZONA 3: output cosmético, excluido de cobertura.

use crate::invoice::Invoice;

pub fn format_invoice(invoice: &Invoice) -> String {
    let mut out = String::new();

    out.push_str(&format!("═══════════════════════════════════════\n"));
    out.push_str(&format!("  INVOICE\n"));
    out.push_str(&format!("═══════════════════════════════════════\n"));
    out.push_str(&format!("  Customer: {} ({})\n",
        invoice.customer.name, invoice.customer.id));
    out.push_str(&format!("  Region:   {}\n", invoice.customer.tax_region));
    out.push_str(&format!("───────────────────────────────────────\n"));

    for item in &invoice.items {
        out.push_str(&format!("  {:20} {:>5} x ${:>8.2} = ${:>10.2}\n",
            item.description, item.quantity, item.unit_price, item.line_total));
    }

    out.push_str(&format!("───────────────────────────────────────\n"));
    out.push_str(&format!("  Subtotal:              ${:>10.2}\n", invoice.subtotal));

    if invoice.discount.amount > 0.0 {
        out.push_str(&format!("  Discount ({:>5.1}%):     -${:>10.2}{}\n",
            invoice.discount.applied_pct * 100.0,
            invoice.discount.amount,
            if invoice.discount.was_capped { " (capped)" } else { "" },
        ));
        out.push_str(&format!("  After discount:        ${:>10.2}\n",
            invoice.subtotal_after_discount));
    }

    out.push_str(&format!("  Tax ({:>5.2}%):          ${:>10.2}\n",
        invoice.tax_rate * 100.0, invoice.tax_amount));
    out.push_str(&format!("═══════════════════════════════════════\n"));
    out.push_str(&format!("  TOTAL:                 ${:>10.2}\n", invoice.total));
    out.push_str(&format!("═══════════════════════════════════════\n"));

    out
}
```

### Resultados de cobertura esperados

Ejecutando `cargo tarpaulin` (con exclusiones configuradas):

```
|| Tested/Total Lines:
|| src/config.rs: 7/8 (87.5%)
|| src/customer.rs: 5/5 (100.0%)
|| src/discount.rs: 28/28 (100.0%)
|| src/invoice.rs: 32/34 (94.1%)
|| src/lib.rs: 10/12 (83.3%)
|| src/tax.rs: 14/14 (100.0%)
||
|| Excluded:
||   src/main.rs (tarpaulin_include)
||   src/report.rs (tarpaulin_include)
||   src/error.rs (thiserror generates Display)
||
|| Total coverage: 96/101 = 95.05%
```

Análisis por zona:

```
┌──────────────────────────────────────────────────────────────────┐
│            Resultados por zona                                    │
│                                                                  │
│  ZONA 1 (crítico):                                               │
│    tax.rs:       100%  ✓  (12 tests)                             │
│    discount.rs:  100%  ✓  (11 tests)                             │
│    invoice.rs:   94%   ✓  (11 tests)                             │
│    Promedio Z1:  98%   ✓  Target: 90%+                           │
│                                                                  │
│  ZONA 2 (soporte):                                               │
│    customer.rs:  100%  ✓  (1 test)                               │
│    config.rs:    87%   ✓  (1 test)                               │
│    lib.rs:       83%   ✓  (testeado transitivamente)             │
│    Promedio Z2:  90%   ✓  Target: 60-80%                         │
│                                                                  │
│  ZONA 3 (excluido):                                              │
│    main.rs:      excluido                                        │
│    report.rs:    excluido                                        │
│    error.rs:     excluido (thiserror)                             │
│                                                                  │
│  Cobertura global (con exclusiones): 95%                         │
│  Cobertura global (sin exclusiones): ~72%                        │
│                                                                  │
│  La diferencia (95% vs 72%) muestra por qué las exclusiones      │
│  son importantes: reflejan la realidad de la confianza.          │
└──────────────────────────────────────────────────────────────────┘
```

---

## 27. Programa de práctica: inventory_pricing

Construye un sistema de pricing de inventario que demuestre las tres zonas de cobertura.

### Especificación

```
inventory_pricing/
├── Cargo.toml
├── .tarpaulin.toml
├── src/
│   ├── main.rs           ← ZONA 3: thin main
│   ├── lib.rs
│   ├── product.rs         ← ZONA 2: modelo de producto
│   ├── pricing.rs         ← ZONA 1: motor de precios
│   ├── stock.rs           ← ZONA 1: control de inventario
│   ├── promotion.rs       ← ZONA 1: engine de promociones
│   ├── error.rs           ← ZONA 3: error types
│   └── display.rs         ← ZONA 3: formateo para terminal
```

### Módulo pricing.rs (ZONA 1) — implementar y testear

```rust
//! Motor de precios — ZONA 1
//! Reglas:
//! 1. Precio base viene del producto
//! 2. Si hay promoción activa, se aplica el descuento de la promoción
//! 3. Si el stock es bajo (< 10), se aplica un markup del 5%
//! 4. Si el stock es crítico (< 3), markup del 15%
//! 5. Los markups NO se aplican si hay promoción activa
//! 6. El precio final se redondea a 2 decimales
//! 7. El precio final nunca puede ser menor que el cost_price

pub fn calculate_final_price(
    product: &Product,
    promotion: Option<&Promotion>,
    current_stock: u32,
) -> Result<PriceResult, PricingError> {
    // Implementar...
    todo!()
}
```

### Módulo stock.rs (ZONA 1) — implementar y testear

```rust
//! Control de inventario — ZONA 1
//! Reglas:
//! 1. No se puede vender más de lo que hay en stock
//! 2. Reservar stock descuenta del disponible
//! 3. Cancelar reserva devuelve al disponible
//! 4. Stock negativo es un bug (panic en debug, error en release)
//! 5. Alertas: low_stock (< 10), critical_stock (< 3), out_of_stock (0)

pub struct StockManager {
    available: HashMap<ProductId, u32>,
    reserved: HashMap<ProductId, u32>,
}

impl StockManager {
    pub fn reserve(&mut self, product_id: &ProductId, qty: u32)
        -> Result<Reservation, StockError> {
        // Implementar...
        todo!()
    }

    pub fn cancel_reservation(&mut self, reservation: Reservation)
        -> Result<(), StockError> {
        // Implementar...
        todo!()
    }

    pub fn stock_level(&self, product_id: &ProductId) -> StockLevel {
        // Implementar...
        todo!()
    }
}
```

### Módulo promotion.rs (ZONA 1) — implementar y testear

```rust
//! Engine de promociones — ZONA 1
//! Reglas:
//! 1. Una promoción tiene fecha inicio y fin
//! 2. Solo se aplica si la fecha actual está en el rango
//! 3. Tipos: porcentaje, monto fijo, "compra N lleva M"
//! 4. No se pueden combinar promociones (se aplica la mejor)
//! 5. Promoción no puede dar descuento > 50%

pub fn best_promotion(
    promotions: &[Promotion],
    product: &Product,
    quantity: u32,
    now: DateTime,
) -> Option<AppliedPromotion> {
    // Implementar...
    todo!()
}
```

### Tareas

1. **Implementa los tres módulos** de Zona 1 con todas las reglas de negocio
2. **Escribe tests exhaustivos** para cada módulo:
   - `pricing.rs`: mínimo 12 tests (todos los casos de markup/promoción/floor)
   - `stock.rs`: mínimo 10 tests (reservas, cancelaciones, alertas, edge cases)
   - `promotion.rs`: mínimo 10 tests (cada tipo, expiración, combinación, cap 50%)
3. **Implementa `product.rs`** (Zona 2) con tests razonables (3-4 tests)
4. **Implementa `error.rs`** (Zona 3) con `thiserror`, sin tests
5. **Implementa `display.rs`** (Zona 3) con formateo de terminal, sin tests
6. **Configura `.tarpaulin.toml`** con exclusiones de Zona 3
7. **Ejecuta `cargo tarpaulin`** y verifica:
   - Zona 1: > 90% line coverage
   - Zona 2: > 60% line coverage
   - Zona 3: excluido del reporte
8. **Ejecuta `cargo mutants` en pricing.rs** y elimina mutantes sobrevivientes
9. **Documenta en un comentario** al inicio de cada archivo su zona y justificación

---

## 28. Ejercicios

### Ejercicio 1: auditoría de cobertura

**Objetivo**: Analizar un proyecto existente y clasificar su código en las tres zonas.

**Tareas**:

**a)** Toma un proyecto Rust que hayas escrito (o usa uno open source pequeño) y
clasifica cada archivo en Zona 1, 2 o 3. Documenta la razón de cada clasificación
en una tabla:

```
| Archivo | Zona | Razón | Cobertura actual | Cobertura target |
|---------|------|-------|------------------|------------------|
| ...     | ...  | ...   | ...              | ...              |
```

**b)** Ejecuta `cargo tarpaulin` sin exclusiones y anota la cobertura global.
Luego configura exclusiones para Zona 3 y ejecuta de nuevo. Compara los dos números
y explica qué refleja mejor la realidad.

**c)** Identifica los tres archivos con menor cobertura. Para cada uno, determina:
- ¿Es un problema real (falta de tests en código crítico)?
- ¿O es ruido (código Zona 3 que infla las métricas negativamente)?

**d)** Si encontraste código Zona 1 con baja cobertura, escribe los tests que faltan.
Si encontraste código Zona 3 sin excluir, agrega las exclusiones.

---

### Ejercicio 2: detección de tests cosméticos

**Objetivo**: Identificar y mejorar tests que dan cobertura sin confianza.

**Tareas**:

**a)** Revisa los tests del ejemplo de facturación (sección 26) y responde:
- ¿Hay algún test cosmético? ¿Cuál y por qué?
- ¿Hay algún test que faltaría para Zona 1?
- ¿Cuántos de los tests de `customer.rs` podrían eliminarse sin perder confianza?

**b)** Escribe una función `detect_cosmetic_test` (en pseudocódigo o Rust) que analice
el código fuente de un test y devuelva una puntuación de 0 a 100 indicando qué tan
"cosmético" es. Considera:
- ¿Tiene `assert!`? ¿Cuántos?
- ¿Los asserts verifican valores específicos o solo `is_ok()`/`is_some()`?
- ¿El test tiene inputs interesantes (edge cases) o solo happy path?
- ¿El nombre del test describe un comportamiento o solo `test_function_name`?

**c)** Ejecuta `cargo mutants` en el ejemplo de facturación. ¿Cuántos mutantes
sobreviven? Para cada mutante sobreviviente, escribe el test que lo mataría.

---

### Ejercicio 3: cobertura realista en C

**Objetivo**: Aplicar los mismos principios de cobertura realista en un proyecto C.

**Tareas**:

**a)** Traduce el módulo `tax.rs` del ejemplo de facturación a C:

```c
// tax.h
typedef enum { TAX_OK, TAX_ERR_INVALID_AMOUNT, TAX_ERR_UNKNOWN_REGION } TaxResult;

TaxResult tax_rate(const char *region, double *rate_out);
TaxResult calculate_tax(double base_amount, const char *region, double *tax_out);
```

Implementa y escribe tests con un framework de testing C (como `check`, `cmocka`,
o simplemente asserts en `main`).

**b)** Compila con `gcc --coverage` y genera el reporte con `gcov`. Compara la
experiencia con `cargo tarpaulin`:
- ¿Cuántos pasos manuales necesitas en C vs Rust?
- ¿El reporte de gcov muestra branch coverage? ¿Con qué flag?
- ¿Cómo excluyes líneas en gcov? (`LCOV_EXCL_LINE`)

**c)** Aplica el patrón "thin main" al programa C. Muestra el `main.c` antes y
después.

---

### Ejercicio 4: pipeline de cobertura completo

**Objetivo**: Diseñar un pipeline de CI que implemente cobertura realista con
umbrales diferenciados.

**Tareas**:

**a)** Escribe un GitHub Actions workflow que:
1. Ejecute `cargo llvm-cov` con exclusiones de Zona 3
2. Genere reporte JSON
3. Ejecute un script que verifique umbrales por zona:
   - Zona 1 (pricing.rs, tax.rs, discount.rs, invoice.rs): >= 90% lines, >= 70% branches
   - Zona 2 (config.rs, customer.rs): >= 60% lines
4. Ejecute `cargo mutants` solo en archivos Zona 1
5. Suba el reporte lcov a Codecov
6. Comente en el PR con un resumen por zona

**b)** Diseña la configuración de `codecov.yml` que implemente:
- Target global: 80%
- Patch coverage (solo líneas nuevas): 90%
- Flag separada para Zona 1 y Zona 2
- No bloquear PRs por Zona 2 baja, solo warn

**c)** Escribe el script `check_coverage.sh` completo que parsea el JSON de llvm-cov
y verifica los umbrales. El script debe:
- Listar cada archivo con su cobertura y zona
- Indicar PASS/FAIL por archivo
- Mostrar un resumen por zona
- Exit code 1 si cualquier archivo Zona 1 está por debajo del umbral
- Exit code 0 si solo archivos Zona 2 están por debajo (con warning)

---

## Navegación

- **Anterior**: [T02 - llvm-cov](../T02_Llvm_cov/README.md)
- **Siguiente**: [C03/S01/T01 - Concepto de fuzzing](../../../C03_Fuzzing_y_Sanitizers/S01_Fuzzing_en_C/T01_Concepto_de_fuzzing/README.md)

---

> **Sección 5: Cobertura en Rust** — Tópico 3 de 3 completado
>
> - T01: cargo-tarpaulin ✓
> - T02: llvm-cov ✓
> - T03: Cobertura realista ✓
>
> **Fin de la Sección 5: Cobertura en Rust**
>
> Resumen de S05:
> 1. **cargo-tarpaulin**: herramienta ergonómica para cobertura rápida, ptrace/LLVM engines, HTML/JSON/lcov output
> 2. **llvm-cov**: instrumentación a nivel LLVM, branch coverage preciso, pipeline manual o cargo-llvm-cov wrapper
> 3. **Cobertura realista**: tres zonas (crítica/soporte/excluir), thin main, mutation testing, umbrales diferenciados
>
> **Fin del Capítulo 2: Testing en Rust**
>
> Resumen de C02:
> - S01: Fundamentos — tests unitarios, assert macros, organización, should_panic/ignore
> - S02: Tests avanzados — integración, propiedades (proptest), parametrización, test fixtures
> - S03: Tests de documentación — doctests, módulo tests, test output, async tests
> - S04: Mocking — trait-based DI, mockall, cuándo no mockear, test doubles sin crates
> - S05: Cobertura — tarpaulin, llvm-cov, cobertura realista
