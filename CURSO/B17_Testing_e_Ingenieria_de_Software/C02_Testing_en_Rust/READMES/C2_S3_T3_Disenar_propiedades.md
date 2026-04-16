# T03 - Diseñar propiedades: el arte de formular invariantes

## Índice

1. [El desafío: pensar en propiedades, no en ejemplos](#1-el-desafío-pensar-en-propiedades-no-en-ejemplos)
2. [Taxonomía completa de propiedades](#2-taxonomía-completa-de-propiedades)
3. [Round-trip properties: encode → decode = identity](#3-round-trip-properties-encode--decode--identity)
4. [Round-trips parciales y asimétricos](#4-round-trips-parciales-y-asimétricos)
5. [Invariantes de resultado: qué debe cumplir la salida](#5-invariantes-de-resultado-qué-debe-cumplir-la-salida)
6. [Invariantes de estructura de datos](#6-invariantes-de-estructura-de-datos)
7. [Propiedades algebraicas: leyes matemáticas](#7-propiedades-algebraicas-leyes-matemáticas)
8. [Modelo oracle: comparar con implementación de referencia](#8-modelo-oracle-comparar-con-implementación-de-referencia)
9. [Idempotencia: f(f(x)) == f(x)](#9-idempotencia-ffx--fx)
10. [Propiedades de preservación](#10-propiedades-de-preservación)
11. [Propiedades de inducción y analogía](#11-propiedades-de-inducción-y-analogía)
12. [Propiedades negativas: qué NO debe ocurrir](#12-propiedades-negativas-qué-no-debe-ocurrir)
13. [Hard to compute, easy to verify](#13-hard-to-compute-easy-to-verify)
14. [Propiedades de rendimiento y recursos](#14-propiedades-de-rendimiento-y-recursos)
15. [Propiedades estadísticas y probabilísticas](#15-propiedades-estadísticas-y-probabilísticas)
16. [El proceso de descubrimiento de propiedades](#16-el-proceso-de-descubrimiento-de-propiedades)
17. [Patrones por dominio: qué propiedades buscar según el problema](#17-patrones-por-dominio-qué-propiedades-buscar-según-el-problema)
18. [De la especificación a las propiedades](#18-de-la-especificación-a-las-propiedades)
19. [Propiedades compuestas: combinar varias propiedades débiles](#19-propiedades-compuestas-combinar-varias-propiedades-débiles)
20. [Trampas al diseñar propiedades](#20-trampas-al-diseñar-propiedades)
21. [Comparación con C y Go](#21-comparación-con-c-y-go)
22. [Errores comunes al diseñar propiedades](#22-errores-comunes-al-diseñar-propiedades)
23. [Ejemplo completo: propiedades para un sistema de permisos](#23-ejemplo-completo-propiedades-para-un-sistema-de-permisos)
24. [Programa de práctica](#24-programa-de-práctica)
25. [Ejercicios](#25-ejercicios)

---

## 1. El desafío: pensar en propiedades, no en ejemplos

El mayor obstáculo al adoptar property-based testing no es técnico (la herramienta), sino **cognitivo**: estamos entrenados para pensar en ejemplos concretos, no en invariantes universales.

### El cambio de mentalidad

```
  PENSAMIENTO BASADO EN EJEMPLOS              PENSAMIENTO BASADO EN PROPIEDADES
  ──────────────────────────────               ──────────────────────────────────
  
  "si ordeno [3,1,2], obtengo [1,2,3]"       "si ordeno cualquier lista, cada
                                                elemento es ≤ al siguiente"
  
  "si codifico 'hola', obtengo 'aG9sYQ=='"   "si codifico y decodifico cualquier
                                                byte[], obtengo el original"
  
  "si sumo 2+3, obtengo 5"                   "si sumo a+b, obtengo lo mismo
                                                que b+a (conmutatividad)"
  
  "si busco 'abc' en 'xabcy', lo encuentro"  "si inserto x en una posición de s,
                                                buscar x en s siempre lo encuentra"
```

### Por qué es difícil

```
┌─────────────────────────────────────────────────────────────────┐
│            RAZONES POR LAS QUE ES DIFÍCIL                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Educación: aprendimos testing con ejemplos                  │
│     "cuando la entrada es X, la salida debe ser Y"              │
│                                                                 │
│  2. Concreción: los ejemplos son tangibles                      │
│     Sabemos qué pasa con sort([3,1,2])                          │
│     Pero ¿qué propiedad tiene sort(cualquier_lista)?            │
│                                                                 │
│  3. Dominio: las propiedades requieren entender                 │
│     las REGLAS del dominio, no solo casos de uso                │
│                                                                 │
│  4. Generalización: ir de lo particular a lo universal          │
│     es un salto de abstracción significativo                    │
│                                                                 │
│  5. Miedo a lo trivial: "¿esta propiedad es demasiado obvia?"  │
│     Las propiedades "obvias" a menudo encuentran bugs           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### El marco mental correcto

En lugar de preguntar **"¿qué resultado espero?"**, pregunta:

1. **"¿Qué relación tiene la entrada con la salida?"** → Round-trip, preservación
2. **"¿Qué debe ser SIEMPRE verdad sobre la salida?"** → Invariantes
3. **"¿Hay otra forma de calcular lo mismo?"** → Oracle
4. **"¿Qué leyes matemáticas aplican?"** → Algebraicas
5. **"¿Qué NO debe ocurrir nunca?"** → Propiedades negativas
6. **"¿Qué se preserva entre entrada y salida?"** → Preservación
7. **"¿Aplicar la operación dos veces cambia algo?"** → Idempotencia

---

## 2. Taxonomía completa de propiedades

```
┌─────────────────────────────────────────────────────────────────┐
│              TAXONOMÍA DE PROPIEDADES                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌── ESTRUCTURALES ──────────────────────────────────────────┐  │
│  │  Round-trip:        f⁻¹(f(x)) = x                        │  │
│  │  Idempotencia:      f(f(x)) = f(x)                       │  │
│  │  Involutividad:     f(f(x)) = x                          │  │
│  │  Preservación:      propiedad(x) → propiedad(f(x))       │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌── ALGEBRAICAS ────────────────────────────────────────────┐  │
│  │  Conmutatividad:    f(a, b) = f(b, a)                    │  │
│  │  Asociatividad:     f(f(a, b), c) = f(a, f(b, c))        │  │
│  │  Identidad:         f(a, e) = a                           │  │
│  │  Distributividad:   f(a, g(b, c)) = g(f(a,b), f(a,c))    │  │
│  │  Absorción:         f(a, z) = z                           │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌── RELACIONALES ───────────────────────────────────────────┐  │
│  │  Oracle:            f(x) = f_referencia(x)                │  │
│  │  Invariante:        invariante(f(x)) siempre es true      │  │
│  │  Monotonía:         x ≤ y → f(x) ≤ f(y)                  │  │
│  │  Inducción:         f(x ++ y) se relaciona con f(x), f(y)│  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌── NEGATIVAS ──────────────────────────────────────────────┐  │
│  │  Nunca panic:       f(x) no lanza panic para ningún x     │  │
│  │  Nunca pierde:      len(f(x)) ≥ len(x) (si aplica)       │  │
│  │  Nunca viola:       f(x) respeta todas las restricciones  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
│  ┌── VERIFICACIÓN ───────────────────────────────────────────┐  │
│  │  Easy to verify:    verificar f(x) es más fácil que       │  │
│  │                     calcular f(x)                         │  │
│  │  Estadísticas:      propiedades que se cumplen "en        │  │
│  │                     promedio" o con alta probabilidad      │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Fortaleza de las propiedades

No todas las propiedades son igual de potentes:

```
  FORTALEZA DE PROPIEDADES (de mayor a menor)
  
  ████████████████████████████████████  Round-trip completo
  Fuerza: MÁXIMA                        f⁻¹(f(x)) = x para todo x
  Si pasa: f y f⁻¹ son correctas       Ejemplo: decode(encode(x)) = x
  
  ██████████████████████████████        Oracle completo
  Fuerza: MUY ALTA                      f(x) = referencia(x)
  Si pasa: f es correcta               Ejemplo: mi_sort(x) = std_sort(x)
  
  ████████████████████████              Conjunto de propiedades algebraicas
  Fuerza: ALTA                          Conmutativa + Asociativa + Identidad
  Si pasa: probablemente correcta       Ejemplo: leyes de un monoide
  
  ██████████████████                    Invariante de salida
  Fuerza: MEDIA                         "la salida siempre cumple P"
  Si pasa: no viola P                   Ejemplo: sort produce lista ordenada
  
  ████████████████                      Preservación
  Fuerza: MEDIA                         "se conserva algo entre E/S"
  Si pasa: no pierde información        Ejemplo: sort preserva elementos
  
  ██████████████                        Propiedad negativa
  Fuerza: MEDIA-BAJA                    "nunca ocurre X"
  Si pasa: X no ocurre                  Ejemplo: nunca panic
  
  ████████████                          Propiedad débil individual
  Fuerza: BAJA                          "la longitud se preserva"
  Si pasa: poco concluyente sola        Ejemplo: sort no cambia len
```

> **Principio clave**: una propiedad débil sola vale poco, pero la COMBINACIÓN de varias propiedades débiles puede ser muy fuerte. Sort preserva longitud (débil) + sort preserva elementos (débil) + sort produce resultado ordenado (débil) = juntas caracterizan sort completamente.

---

## 3. Round-trip properties: encode → decode = identity

La propiedad más poderosa y frecuente. Si tienes dos funciones que son "inversas" una de la otra, la composición debe ser la identidad.

### Forma general

```
  Round-trip completo:
  
    ∀ x ∈ Dominio:  decode(encode(x)) = x
    
    ┌─────┐     encode     ┌─────────┐     decode     ┌─────┐
    │  x  │ ──────────────→│ encoded │ ──────────────→│  x  │
    └─────┘                └─────────┘                └─────┘
      │                                                  │
      └──────────────── deben ser iguales ───────────────┘
```

### Ejemplos clásicos de round-trip

```rust
use proptest::prelude::*;

// ── Serialización / Deserialización ──────────────────────────

proptest! {
    #[test]
    fn roundtrip_json(value in any::<i64>()) {
        let json = serde_json::to_string(&value).unwrap();
        let recovered: i64 = serde_json::from_str(&json).unwrap();
        prop_assert_eq!(value, recovered);
    }
}

// ── Compresión / Descompresión ───────────────────────────────

proptest! {
    #[test]
    fn roundtrip_compression(
        data in proptest::collection::vec(any::<u8>(), 0..500)
    ) {
        let compressed = compress(&data);
        let decompressed = decompress(&compressed).unwrap();
        prop_assert_eq!(data, decompressed);
    }
}

// ── Parsing / Formatting ────────────────────────────────────

proptest! {
    #[test]
    fn roundtrip_ip_address(
        a in 0..=255u8, b in 0..=255u8,
        c in 0..=255u8, d in 0..=255u8
    ) {
        let ip = format!("{}.{}.{}.{}", a, b, c, d);
        let parsed = parse_ip(&ip).unwrap();
        let formatted = format_ip(&parsed);
        prop_assert_eq!(ip, formatted);
    }
}

// ── Cifrado / Descifrado ────────────────────────────────────

proptest! {
    #[test]
    fn roundtrip_encryption(
        plaintext in proptest::collection::vec(any::<u8>(), 1..200),
        key in proptest::collection::vec(any::<u8>(), 32)  // 256-bit key
    ) {
        let ciphertext = encrypt(&plaintext, &key);
        let decrypted = decrypt(&ciphertext, &key).unwrap();
        prop_assert_eq!(plaintext, decrypted);
    }
}

// ── Conversión de tipos ─────────────────────────────────────

proptest! {
    #[test]
    fn roundtrip_utf8(s in any::<String>()) {
        let bytes = s.as_bytes().to_vec();
        let recovered = String::from_utf8(bytes).unwrap();
        prop_assert_eq!(s, recovered);
    }
}
```

### El poder del round-trip: un test, dos funciones

```
  Un round-trip verifica AMBAS funciones simultáneamente:
  
  ┌────────────────────────────────────────────────────────────┐
  │                                                            │
  │  Si encode tiene un bug:                                   │
  │    encode(x) → datos corruptos → decode falla              │
  │    → decode(encode(x)) ≠ x → TEST FALLA ✓                │
  │                                                            │
  │  Si decode tiene un bug:                                   │
  │    encode(x) → datos correctos → decode malinterpreta      │
  │    → decode(encode(x)) ≠ x → TEST FALLA ✓                │
  │                                                            │
  │  Si ambos tienen bugs "compensatorios":                    │
  │    Probabilidad ≈ 0 de que bugs opuestos se cancelen       │
  │    exactamente para TODOS los inputs aleatorios             │
  │    → Casi seguro: TEST FALLA ✓                            │
  │                                                            │
  │  Corolario: con UN test verificas DOS funciones            │
  │                                                            │
  └────────────────────────────────────────────────────────────┘
```

### Implementación robusta en Rust

```rust
use proptest::prelude::*;

// Función genérica para testear round-trips
fn assert_roundtrip<T, E, F, G>(
    value: &T,
    encode: F,
    decode: G,
) -> Result<(), TestCaseError>
where
    T: std::fmt::Debug + PartialEq,
    E: std::fmt::Debug,
    F: Fn(&T) -> E,
    G: Fn(&E) -> Result<T, String>,
{
    let encoded = encode(value);
    let decoded = decode(&encoded)
        .map_err(|e| TestCaseError::fail(format!("decode failed: {}", e)))?;
    prop_assert_eq!(
        value, &decoded,
        "roundtrip failed:\n  original: {:?}\n  encoded:  {:?}\n  decoded:  {:?}",
        value, encoded, decoded
    );
    Ok(())
}
```

---

## 4. Round-trips parciales y asimétricos

No siempre se puede hacer un round-trip completo. A veces la codificación pierde información o la normaliza.

### Round-trip parcial: solo una dirección

```
  Round-trip completo (bidireccional):
    decode(encode(x)) = x        ← para TODO x
    encode(decode(y)) = y        ← para TODO y válido
    
  Round-trip parcial (una dirección):
    decode(encode(x)) = x        ← funciona
    encode(decode(y)) = y        ← NO siempre funciona
    
  Ejemplo: formatear floats
    format(parse("1.0")) = "1"   ← cambia la representación
    parse(format(1.0)) = 1.0     ← esta dirección SÍ funciona
```

### Ejemplos de round-trips parciales

```rust
use proptest::prelude::*;

// ── Normalización: encode pierde la forma original ──────────

proptest! {
    #[test]
    fn partial_roundtrip_whitespace(s in "[a-z ]{1,30}") {
        // normalize_whitespace colapsa espacios múltiples a uno
        let normalized = normalize_whitespace(&s);
        
        // Aplicar de nuevo no cambia nada (idempotencia)
        let double = normalize_whitespace(&normalized);
        prop_assert_eq!(normalized, double);
        
        // PERO: normalize(s) ≠ s en general
        // No podemos verificar: original == normalize(original)
    }
}

// ── Lossy conversion: se pierde información ──────────────────

proptest! {
    #[test]
    fn partial_roundtrip_lowercase(s in "[a-zA-Z]{1,20}") {
        let lower = s.to_lowercase();
        
        // No podemos recuperar mayúsculas originales
        // Pero: lowercase es idempotente
        prop_assert_eq!(lower, lower.to_lowercase());
        
        // Y: lowercase preserva la longitud (para ASCII)
        prop_assert_eq!(s.len(), lower.len());
    }
}

// ── Canonicalización: múltiples inputs → mismo output ────────

proptest! {
    #[test]
    fn canonical_form(
        // Generar paths con componentes redundantes
        parts in proptest::collection::vec("[a-z]{1,5}", 2..6),
    ) {
        let path = parts.join("/");
        let with_dots = format!("./{}/.", path);
        let with_double = path.replace("/", "//");
        
        let canon1 = canonicalize_path(&path);
        let canon2 = canonicalize_path(&with_dots);
        let canon3 = canonicalize_path(&with_double);
        
        // Múltiples representaciones → misma forma canónica
        prop_assert_eq!(&canon1, &canon2);
        prop_assert_eq!(&canon1, &canon3);
        
        // Canonicalizar es idempotente
        prop_assert_eq!(canon1, canonicalize_path(&canon1));
    }
}
```

### Diagrama de tipos de round-trip

```
  ROUND-TRIP COMPLETO (biyección):
  
    A ──encode──→ B ──decode──→ A
    │                           │
    └───── decode(encode(a))=a ─┘      ∀ a ∈ A
    
    B ──decode──→ A ──encode──→ B
    │                           │
    └───── encode(decode(b))=b ─┘      ∀ b ∈ B válido
    
    Ejemplo: UTF-8 ←→ String (sin bytes inválidos)
    
    
  ROUND-TRIP PARCIAL (solo una dirección):
  
    A ──encode──→ B ──decode──→ A      ✓ funciona
    B ──decode──→ A ──encode──→ B      ✗ puede fallar
    
    Ejemplo: i32 → String → i32
      42  → "42" → 42    ✓
      "042" → 42 → "42"  ✗ (pierde el cero inicial)
    
    
  ROUND-TRIP CON NORMALIZACIÓN:
  
    A ──encode──→ B ──decode──→ A'     donde A' = normalize(A)
    
    Propiedad: decode(encode(x)) = normalize(x)
    
    Ejemplo: JSON con pretty-print
      {"a":1,"b":2} → pretty → {"a":1, "b":2}
      decode(encode(x)) = normalize_json(x)
    
    
  ROUND-TRIP ESTADÍSTICO (lossy):
  
    A ──encode──→ B ──decode──→ A'     donde |A - A'| < ε
    
    Propiedad: distance(x, decode(encode(x))) < epsilon
    
    Ejemplo: compresión con pérdida (JPEG, MP3)
      imagen → compress → decompress → imagen_similar
      diff(original, recovered) < threshold
```

### Cómo testear un round-trip parcial

```rust
use proptest::prelude::*;

// Cuando encode pierde información, testear la versión débil
proptest! {
    #[test]
    fn weak_roundtrip_float_format(x in proptest::num::f64::NORMAL) {
        let formatted = format!("{}", x);
        let parsed: f64 = formatted.parse().unwrap();
        
        // No podemos esperar igualdad exacta de strings
        // pero sí igualdad numérica (con tolerancia)
        let diff = (x - parsed).abs();
        let tolerance = x.abs() * 1e-15;  // error relativo
        prop_assert!(
            diff <= tolerance || diff < 1e-300,
            "x={}, parsed={}, diff={}", x, parsed, diff
        );
    }
}

// Cuando hay normalización, testear la forma canónica
proptest! {
    #[test]
    fn normalized_roundtrip(
        email in "[A-Za-z][a-zA-Z0-9.]{1,10}@[a-z]{3,8}\\.(com|org)"
    ) {
        let normalized = normalize_email(&email);  // lowercase
        let parsed = parse_email(&normalized).unwrap();
        let re_normalized = normalize_email(&format_email(&parsed));
        
        // Después de normalizar, el round-trip funciona
        prop_assert_eq!(normalized, re_normalized);
    }
}
```

---

## 5. Invariantes de resultado: qué debe cumplir la salida

Una invariante de resultado dice: "sin importar qué input le des, la salida SIEMPRE cumple la propiedad P".

### Invariantes de sort

```rust
use proptest::prelude::*;

fn is_sorted(v: &[i32]) -> bool {
    v.windows(2).all(|w| w[0] <= w[1])
}

proptest! {
    #![proptest_config(ProptestConfig::with_cases(1000))]

    #[test]
    fn sort_produces_sorted_output(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        
        // INVARIANTE: la salida está ordenada
        prop_assert!(
            is_sorted(&sorted),
            "sort no produjo resultado ordenado: {:?}", sorted
        );
    }
    
    #[test]
    fn sort_preserves_length(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        
        // INVARIANTE: la longitud se preserva
        prop_assert_eq!(v.len(), sorted.len());
    }
    
    #[test]
    fn sort_preserves_elements(
        v in proptest::collection::vec(any::<i32>(), 0..100)
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        
        // INVARIANTE: los mismos elementos, posiblemente reordenados
        let mut original = v;
        original.sort();
        prop_assert_eq!(sorted, original);
    }
    
    #[test]
    fn sort_minimum_is_first(
        v in proptest::collection::vec(any::<i32>(), 1..100)
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        
        // INVARIANTE: el primer elemento es el mínimo
        let min = v.iter().min().unwrap();
        prop_assert_eq!(sorted[0], *min);
    }
    
    #[test]
    fn sort_maximum_is_last(
        v in proptest::collection::vec(any::<i32>(), 1..100)
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        
        // INVARIANTE: el último elemento es el máximo
        let max = v.iter().max().unwrap();
        prop_assert_eq!(sorted[sorted.len() - 1], *max);
    }
}
```

### ¿Estas 5 propiedades juntas caracterizan sort?

```
  Propiedad                        Qué atrapa
  ─────────────────────────────    ──────────────────────────
  Resultado ordenado               ✓ Output está en orden
  Preserva longitud                ✓ No agrega ni quita
  Preserva elementos               ✓ No inventa ni pierde
  Mínimo al inicio                 ✓ Caso edge del inicio
  Máximo al final                  ✓ Caso edge del final
  
  ¿Son suficientes JUNTAS?
  
  Sí — una función que:
    1. Produce output ordenado
    2. Con los mismos elementos que el input
  ES un sort correcto. Las demás propiedades son redundantes
  pero sirven como diagnóstico si algo falla.
  
  Función que pasa 1 pero no 2: retorna [1,2,3,...,n] siempre
  Función que pasa 2 pero no 1: retorna el input sin cambios
  Función que pasa 1+2: ES un sort correcto ✓
```

### Invariantes para otros algoritmos

```rust
// ── Búsqueda binaria ─────────────────────────────────────────

proptest! {
    #[test]
    fn binary_search_correct(
        v in proptest::collection::vec(any::<i32>(), 0..50)
            .prop_map(|mut v| { v.sort(); v.dedup(); v }),
        target in any::<i32>()
    ) {
        let result = v.binary_search(&target);
        
        match result {
            Ok(idx) => {
                // INVARIANTE: si encontró, el elemento está ahí
                prop_assert_eq!(v[idx], target);
            }
            Err(idx) => {
                // INVARIANTE: si no encontró, idx es donde se insertaría
                // para mantener el orden
                if idx > 0 {
                    prop_assert!(v[idx - 1] < target);
                }
                if idx < v.len() {
                    prop_assert!(v[idx] > target);
                }
            }
        }
    }
}

// ── Filtrado ─────────────────────────────────────────────────

proptest! {
    #[test]
    fn filter_invariants(
        v in proptest::collection::vec(any::<i32>(), 0..50),
        threshold in any::<i32>()
    ) {
        let filtered: Vec<i32> = v.iter()
            .filter(|&&x| x > threshold)
            .copied()
            .collect();
        
        // INVARIANTE 1: todos los elementos pasan el filtro
        for &x in &filtered {
            prop_assert!(x > threshold);
        }
        
        // INVARIANTE 2: no se perdió ningún elemento que debía pasar
        let count_expected = v.iter().filter(|&&x| x > threshold).count();
        prop_assert_eq!(filtered.len(), count_expected);
        
        // INVARIANTE 3: el orden relativo se preserva
        for i in 1..filtered.len() {
            let pos_prev = v.iter().position(|&x| x == filtered[i-1]).unwrap();
            let pos_curr = v.iter().rposition(|&x| x == filtered[i]).unwrap();
            // Relajamos: puede haber duplicados, pero el orden general se mantiene
            // En una implementación sin duplicados, sería pos_prev < pos_curr
        }
        
        // INVARIANTE 4: resultado es subconjunto del original
        prop_assert!(filtered.len() <= v.len());
    }
}

// ── Deduplicación ────────────────────────────────────────────

proptest! {
    #[test]
    fn dedup_invariants(
        v in proptest::collection::vec(0..20i32, 0..50)
    ) {
        let mut sorted = v.clone();
        sorted.sort();
        sorted.dedup();
        
        // INVARIANTE 1: sin duplicados
        let as_set: std::collections::HashSet<_> = sorted.iter().collect();
        prop_assert_eq!(sorted.len(), as_set.len());
        
        // INVARIANTE 2: mismos valores únicos
        let original_set: std::collections::HashSet<_> = v.iter().collect();
        let sorted_set: std::collections::HashSet<_> = sorted.iter().collect();
        prop_assert_eq!(original_set, sorted_set);
        
        // INVARIANTE 3: resultado sigue ordenado
        prop_assert!(sorted.windows(2).all(|w| w[0] < w[1]));
        
        // INVARIANTE 4: tamaño ≤ original
        prop_assert!(sorted.len() <= v.len());
    }
}
```

---

## 6. Invariantes de estructura de datos

Las estructuras de datos tienen invariantes internas que deben mantenerse después de CUALQUIER secuencia de operaciones.

### Invariantes típicas por estructura

```
┌──────────────────┬──────────────────────────────────────────────┐
│ Estructura       │ Invariantes                                  │
├──────────────────┼──────────────────────────────────────────────┤
│ BST              │ ∀ nodo: izq < nodo < der                    │
│                  │ in-order traversal está ordenado              │
├──────────────────┼──────────────────────────────────────────────┤
│ Heap (min)       │ ∀ nodo: padre ≤ hijos                       │
│                  │ raíz = mínimo global                         │
│                  │ es un árbol completo                         │
├──────────────────┼──────────────────────────────────────────────┤
│ Red-Black Tree   │ Raíz es negra                                │
│                  │ Hijos de rojo son negros                     │
│                  │ Mismo número de negros en cada camino         │
├──────────────────┼──────────────────────────────────────────────┤
│ AVL Tree         │ |altura(izq) - altura(der)| ≤ 1 para c/nodo │
│                  │ es un BST                                    │
├──────────────────┼──────────────────────────────────────────────┤
│ Hash Table       │ load_factor ≤ umbral                        │
│                  │ ∀ (k,v): table[hash(k)] contiene (k,v)      │
│                  │ sin claves duplicadas                        │
├──────────────────┼──────────────────────────────────────────────┤
│ Sorted Vec       │ ∀ i: v[i] ≤ v[i+1]                          │
│                  │ búsqueda binaria funciona correctamente      │
├──────────────────┼──────────────────────────────────────────────┤
│ Ring Buffer      │ head y tail dentro del rango                 │
│                  │ size = (tail - head) mod capacity            │
│                  │ size ≤ capacity                              │
├──────────────────┼──────────────────────────────────────────────┤
│ Intervalo [a,b]  │ a ≤ b                                        │
│                  │ IntervalSet: intervalos disjuntos y ordenados │
└──────────────────┴──────────────────────────────────────────────┘
```

### Ejemplo: invariantes de un BST

```rust
use proptest::prelude::*;

#[derive(Debug, Clone)]
enum BST {
    Empty,
    Node {
        value: i32,
        left: Box<BST>,
        right: Box<BST>,
    },
}

impl BST {
    fn insert(&mut self, val: i32) {
        match self {
            BST::Empty => {
                *self = BST::Node {
                    value: val,
                    left: Box::new(BST::Empty),
                    right: Box::new(BST::Empty),
                };
            }
            BST::Node { value, left, right } => {
                if val < *value {
                    left.insert(val);
                } else if val > *value {
                    right.insert(val);
                }
                // val == *value: ya existe, no insertar
            }
        }
    }

    fn contains(&self, val: i32) -> bool {
        match self {
            BST::Empty => false,
            BST::Node { value, left, right } => {
                if val == *value {
                    true
                } else if val < *value {
                    left.contains(val)
                } else {
                    right.contains(val)
                }
            }
        }
    }

    fn in_order(&self) -> Vec<i32> {
        match self {
            BST::Empty => vec![],
            BST::Node { value, left, right } => {
                let mut result = left.in_order();
                result.push(*value);
                result.extend(right.in_order());
                result
            }
        }
    }

    fn is_valid_bst(&self) -> bool {
        self.is_valid_bst_range(i32::MIN, i32::MAX)
    }

    fn is_valid_bst_range(&self, min: i32, max: i32) -> bool {
        match self {
            BST::Empty => true,
            BST::Node { value, left, right } => {
                *value >= min
                    && *value <= max
                    && left.is_valid_bst_range(min, value.saturating_sub(1))
                    && right.is_valid_bst_range(value.saturating_add(1), max)
            }
        }
    }
}

proptest! {
    #![proptest_config(ProptestConfig::with_cases(500))]

    // INVARIANTE 1: siempre es un BST válido
    #[test]
    fn bst_always_valid(
        values in proptest::collection::vec(-1000..1000i32, 0..30)
    ) {
        let mut tree = BST::Empty;
        for &v in &values {
            tree.insert(v);
            prop_assert!(
                tree.is_valid_bst(),
                "BST inválido después de insertar {}", v
            );
        }
    }

    // INVARIANTE 2: in-order traversal siempre está ordenado
    #[test]
    fn bst_inorder_sorted(
        values in proptest::collection::vec(-1000..1000i32, 0..30)
    ) {
        let mut tree = BST::Empty;
        for &v in &values {
            tree.insert(v);
        }
        
        let in_order = tree.in_order();
        prop_assert!(
            in_order.windows(2).all(|w| w[0] < w[1]),
            "in-order no está estrictamente ordenado: {:?}", in_order
        );
    }

    // INVARIANTE 3: todo lo insertado se puede encontrar
    #[test]
    fn bst_contains_inserted(
        values in proptest::collection::vec(-1000..1000i32, 1..30)
    ) {
        let mut tree = BST::Empty;
        for &v in &values {
            tree.insert(v);
        }
        
        for &v in &values {
            prop_assert!(
                tree.contains(v),
                "BST no contiene {} que fue insertado", v
            );
        }
    }

    // INVARIANTE 4: in-order contiene exactamente los valores únicos
    #[test]
    fn bst_inorder_has_all_unique(
        values in proptest::collection::vec(-1000..1000i32, 0..30)
    ) {
        let mut tree = BST::Empty;
        for &v in &values {
            tree.insert(v);
        }
        
        let in_order = tree.in_order();
        let mut unique: Vec<i32> = values;
        unique.sort();
        unique.dedup();
        
        prop_assert_eq!(in_order, unique);
    }
}
```

---

## 7. Propiedades algebraicas: leyes matemáticas

Cuando tu código implementa operaciones que tienen estructura algebraica (grupos, monoides, anillos, etc.), las leyes algebraicas son propiedades naturales.

### Leyes fundamentales

```
  ┌── CONMUTATIVIDAD ──────────────────────────────────────────┐
  │  f(a, b) = f(b, a)                                        │
  │                                                            │
  │  Aplica a: suma, multiplicación, max, min, unión,          │
  │            intersección, OR, AND, XOR                       │
  │  NO aplica a: resta, división, concatenación, composición  │
  └────────────────────────────────────────────────────────────┘
  
  ┌── ASOCIATIVIDAD ───────────────────────────────────────────┐
  │  f(f(a, b), c) = f(a, f(b, c))                            │
  │                                                            │
  │  Aplica a: suma, multiplicación, concat, unión, AND, OR    │
  │  NO aplica a: resta (a-(b-c) ≠ (a-b)-c), potencia         │
  └────────────────────────────────────────────────────────────┘
  
  ┌── ELEMENTO IDENTIDAD ─────────────────────────────────────┐
  │  f(a, e) = a = f(e, a)                                    │
  │                                                            │
  │  Suma: e=0        Multiplicación: e=1                      │
  │  Concat: e=""     Unión: e=∅        AND: e=true            │
  │  OR: e=false      Max: e=-∞         Min: e=+∞              │
  └────────────────────────────────────────────────────────────┘
  
  ┌── ELEMENTO ABSORBENTE ────────────────────────────────────┐
  │  f(a, z) = z = f(z, a)                                    │
  │                                                            │
  │  Multiplicación: z=0    AND: z=false    Intersección: z=∅  │
  └────────────────────────────────────────────────────────────┘
  
  ┌── DISTRIBUTIVIDAD ────────────────────────────────────────┐
  │  f(a, g(b, c)) = g(f(a, b), f(a, c))                     │
  │                                                            │
  │  * distribuye sobre +: a*(b+c) = a*b + a*c                 │
  │  ∩ distribuye sobre ∪: A∩(B∪C) = (A∩B)∪(A∩C)              │
  │  AND distribuye sobre OR                                   │
  └────────────────────────────────────────────────────────────┘
  
  ┌── INVOLUTIVIDAD ──────────────────────────────────────────┐
  │  f(f(x)) = x                                              │
  │                                                            │
  │  Negación: --x = x     NOT: !!x = x     Reverso: rev(rev) │
  │  Transpose: T(T(M))=M  Complemento: ~~A = A               │
  └────────────────────────────────────────────────────────────┘
```

### Ejemplo: propiedades de una operación de conjuntos

```rust
use proptest::prelude::*;
use std::collections::BTreeSet;

fn set_strategy() -> impl Strategy<Value = BTreeSet<i32>> {
    proptest::collection::btree_set(-100..100i32, 0..20)
}

proptest! {
    // ── Conmutatividad ───────────────────────────────────────
    
    #[test]
    fn union_commutative(a in set_strategy(), b in set_strategy()) {
        let ab: BTreeSet<_> = a.union(&b).copied().collect();
        let ba: BTreeSet<_> = b.union(&a).copied().collect();
        prop_assert_eq!(ab, ba);
    }
    
    #[test]
    fn intersection_commutative(a in set_strategy(), b in set_strategy()) {
        let ab: BTreeSet<_> = a.intersection(&b).copied().collect();
        let ba: BTreeSet<_> = b.intersection(&a).copied().collect();
        prop_assert_eq!(ab, ba);
    }
    
    // ── Asociatividad ────────────────────────────────────────
    
    #[test]
    fn union_associative(
        a in set_strategy(), b in set_strategy(), c in set_strategy()
    ) {
        let ab: BTreeSet<_> = a.union(&b).copied().collect();
        let ab_c: BTreeSet<_> = ab.union(&c).copied().collect();
        
        let bc: BTreeSet<_> = b.union(&c).copied().collect();
        let a_bc: BTreeSet<_> = a.union(&bc).copied().collect();
        
        prop_assert_eq!(ab_c, a_bc);
    }
    
    // ── Identidad ────────────────────────────────────────────
    
    #[test]
    fn union_identity(a in set_strategy()) {
        let empty = BTreeSet::new();
        let result: BTreeSet<_> = a.union(&empty).copied().collect();
        prop_assert_eq!(result, a);
    }
    
    #[test]
    fn intersection_with_self(a in set_strategy()) {
        let result: BTreeSet<_> = a.intersection(&a).copied().collect();
        prop_assert_eq!(result, a);
    }
    
    // ── Absorción ────────────────────────────────────────────
    
    #[test]
    fn intersection_empty(a in set_strategy()) {
        let empty = BTreeSet::new();
        let result: BTreeSet<_> = a.intersection(&empty).copied().collect();
        prop_assert!(result.is_empty());
    }
    
    // ── Distributividad ──────────────────────────────────────
    
    #[test]
    fn intersection_distributes_over_union(
        a in set_strategy(), b in set_strategy(), c in set_strategy()
    ) {
        // A ∩ (B ∪ C) = (A ∩ B) ∪ (A ∩ C)
        let b_union_c: BTreeSet<_> = b.union(&c).copied().collect();
        let lhs: BTreeSet<_> = a.intersection(&b_union_c).copied().collect();
        
        let a_inter_b: BTreeSet<_> = a.intersection(&b).copied().collect();
        let a_inter_c: BTreeSet<_> = a.intersection(&c).copied().collect();
        let rhs: BTreeSet<_> = a_inter_b.union(&a_inter_c).copied().collect();
        
        prop_assert_eq!(lhs, rhs);
    }
    
    // ── Leyes de De Morgan ───────────────────────────────────
    
    #[test]
    fn de_morgan_union(
        a in set_strategy(),
        b in set_strategy(),
        universe in set_strategy()
    ) {
        // Complemento respecto al universo
        let complement = |s: &BTreeSet<i32>, u: &BTreeSet<i32>| -> BTreeSet<i32> {
            u.difference(s).copied().collect()
        };
        
        // ~(A ∪ B) = ~A ∩ ~B
        let a_union_b: BTreeSet<_> = a.union(&b).copied().collect();
        let lhs = complement(&a_union_b, &universe);
        
        let comp_a = complement(&a, &universe);
        let comp_b = complement(&b, &universe);
        let rhs: BTreeSet<_> = comp_a.intersection(&comp_b).copied().collect();
        
        prop_assert_eq!(lhs, rhs);
    }
}
```

### Identificar propiedades algebraicas en tu código

```
  Pregunta                                   Propiedad potencial
  ──────────────────────────────────────     ─────────────────────
  ¿merge(A, B) = merge(B, A)?               Conmutatividad
  ¿merge(merge(A,B), C) = merge(A,merge(B,C))?  Asociatividad
  ¿Hay un valor "vacío" que no cambia nada?  Identidad
  ¿Hay un valor que "anula" todo?            Absorción
  ¿Aplicar dos veces deshace?               Involutividad
  ¿f distribuye sobre g?                     Distributividad
  ¿x ≤ y implica f(x) ≤ f(y)?              Monotonía
```

---

## 8. Modelo oracle: comparar con implementación de referencia

Un **oracle** es una implementación correcta (pero posiblemente ineficiente) contra la que compararas tu implementación optimizada.

### El concepto

```
  ┌─────────────────────────────────────────────────────────────┐
  │                    ORACLE TESTING                           │
  │                                                             │
  │  Input x                                                    │
  │    │                                                        │
  │    ├──→ Implementación a testear (SUT) ──→ resultado_sut    │
  │    │                                                        │
  │    └──→ Implementación de referencia ────→ resultado_oracle │
  │         (oracle)                                            │
  │                                                             │
  │  Verificar: resultado_sut == resultado_oracle               │
  │                                                             │
  │  El oracle puede ser:                                       │
  │  ├── La librería estándar (HashMap vs tu hash map)          │
  │  ├── Una implementación naïve pero obvia                    │
  │  ├── Un lenguaje de referencia (Python, etc.)               │
  │  ├── Una versión anterior del código                        │
  │  └── Una fórmula matemática cerrada                        │
  └─────────────────────────────────────────────────────────────┘
```

### Ejemplo: sort optimizado vs sort de referencia

```rust
use proptest::prelude::*;

// Tu implementación optimizada
fn my_sort(v: &mut [i32]) {
    // Implementación custom (quicksort, radix sort, etc.)
    // ... (posiblemente con bugs)
    v.sort(); // placeholder
}

proptest! {
    #[test]
    fn my_sort_matches_std(
        v in proptest::collection::vec(any::<i32>(), 0..200)
    ) {
        // Oracle: sort de la librería estándar
        let mut expected = v.clone();
        expected.sort();
        
        // SUT: tu implementación
        let mut actual = v;
        my_sort(&mut actual);
        
        // Deben producir el mismo resultado
        prop_assert_eq!(actual, expected);
    }
}
```

### Ejemplo: parser optimizado vs parser naïve

```rust
use proptest::prelude::*;

// Implementación naïve (obvia pero lenta)
fn parse_csv_naive(input: &str) -> Vec<Vec<String>> {
    input
        .lines()
        .map(|line| {
            line.split(',')
                .map(|field| field.trim().to_string())
                .collect()
        })
        .collect()
}

// Implementación optimizada (rápida pero ¿correcta?)
fn parse_csv_fast(input: &str) -> Vec<Vec<String>> {
    // Implementación con SIMD, zero-copy, etc.
    parse_csv_naive(input) // placeholder
}

proptest! {
    #[test]
    fn fast_csv_matches_naive(
        // Generar CSVs aleatorios
        rows in proptest::collection::vec(
            proptest::collection::vec("[a-zA-Z0-9 ]{0,10}", 1..6)
                .prop_map(|fields| fields.join(",")),
            1..20
        ).prop_map(|rows| rows.join("\n"))
    ) {
        let naive = parse_csv_naive(&rows);
        let fast = parse_csv_fast(&rows);
        
        prop_assert_eq!(naive, fast);
    }
}
```

### Ejemplo: estructura de datos custom vs estándar

```rust
use proptest::prelude::*;
use std::collections::BTreeMap;

// Tu implementación de map ordenado
struct MyOrderedMap {
    data: Vec<(i32, String)>,
}

impl MyOrderedMap {
    fn new() -> Self { MyOrderedMap { data: Vec::new() } }
    
    fn insert(&mut self, key: i32, value: String) {
        match self.data.binary_search_by_key(&key, |(k, _)| *k) {
            Ok(idx) => self.data[idx].1 = value,
            Err(idx) => self.data.insert(idx, (key, value)),
        }
    }
    
    fn get(&self, key: &i32) -> Option<&String> {
        self.data
            .binary_search_by_key(key, |(k, _)| *k)
            .ok()
            .map(|idx| &self.data[idx].1)
    }
    
    fn remove(&mut self, key: &i32) -> Option<String> {
        match self.data.binary_search_by_key(key, |(k, _)| *k) {
            Ok(idx) => Some(self.data.remove(idx).1),
            Err(_) => None,
        }
    }
}

#[derive(Debug, Clone)]
enum MapOp {
    Insert(i32, String),
    Get(i32),
    Remove(i32),
}

fn map_op_strategy() -> impl Strategy<Value = MapOp> {
    prop_oneof![
        (any::<i32>(), "[a-z]{1,5}")
            .prop_map(|(k, v)| MapOp::Insert(k, v)),
        any::<i32>().prop_map(MapOp::Get),
        any::<i32>().prop_map(MapOp::Remove),
    ]
}

proptest! {
    #[test]
    fn my_map_matches_btreemap(
        ops in proptest::collection::vec(map_op_strategy(), 1..100)
    ) {
        let mut oracle = BTreeMap::new();
        let mut sut = MyOrderedMap::new();
        
        for op in &ops {
            match op {
                MapOp::Insert(k, v) => {
                    oracle.insert(*k, v.clone());
                    sut.insert(*k, v.clone());
                }
                MapOp::Get(k) => {
                    let expected = oracle.get(k);
                    let actual = sut.get(k);
                    prop_assert_eq!(
                        actual, expected,
                        "get({}) divergió: sut={:?}, oracle={:?}",
                        k, actual, expected
                    );
                }
                MapOp::Remove(k) => {
                    let expected = oracle.remove(k);
                    let actual = sut.remove(k);
                    prop_assert_eq!(
                        actual, expected,
                        "remove({}) divergió",
                        k
                    );
                }
            }
        }
    }
}
```

### Cuándo usar oracle testing

```
  ✅ BUEN CANDIDATO PARA ORACLE:
  
  ├── Optimizaciones de rendimiento
  │   "Mi quicksort es correcto?" → comparar con std::sort
  │
  ├── Reimplementaciones
  │   "Mi JSON parser es correcto?" → comparar con serde_json
  │
  ├── Algoritmos con versión simple
  │   "Mi BFS paralelo es correcto?" → comparar con BFS secuencial
  │
  ├── Estructuras de datos custom
  │   "Mi B-tree es correcto?" → comparar con BTreeMap
  │
  └── Migración de lenguaje
      "Mi port de Python a Rust es correcto?" → comparar outputs
  
  
  ❌ MAL CANDIDATO PARA ORACLE:
  
  ├── No existe implementación de referencia
  │   (entonces usa invariantes de salida)
  │
  ├── El oracle ES la implementación
  │   (estás replicando el código, no testeando)
  │
  └── El oracle tiene bugs conocidos
      (vas a "verificar" contra errores)
```

---

## 9. Idempotencia: f(f(x)) == f(x)

Una operación es **idempotente** si aplicarla más de una vez produce el mismo resultado que aplicarla una vez. Es una propiedad sorprendentemente común y fácil de testear.

### Forma general

```
  Idempotencia:
    f(f(x)) = f(x)     para todo x
    
  Equivalente a:
    Sea y = f(x).  Entonces f(y) = y.
    "Los resultados de f son puntos fijos de f."
    
  ┌────────────────────────────────────────────────────────────┐
  │  x ──f──→ f(x) ──f──→ f(f(x))                            │
  │            │              │                                │
  │            └── iguales ───┘                                │
  │                                                            │
  │  Primera aplicación puede cambiar x                        │
  │  Segunda aplicación NO cambia nada                         │
  └────────────────────────────────────────────────────────────┘
```

### Operaciones idempotentes comunes

```
┌───────────────────────────┬──────────────────────────────────┐
│ Operación                 │ Por qué es idempotente           │
├───────────────────────────┼──────────────────────────────────┤
│ sort(sort(v)) = sort(v)   │ Ordenar algo ya ordenado no hace │
│                           │ nada                             │
│ dedup(dedup(v)) = dedup(v)│ Deduplicar sin dups no cambia    │
│ trim(trim(s)) = trim(s)   │ Trimear sin espacios no cambia   │
│ abs(abs(x)) = abs(x)      │ Absoluto de positivo = positivo  │
│ max(a, max(a,b)) = max(a,b)│ Max es idempotente              │
│ normalize(normalize(s))   │ Normalizar ya normalizado =      │
│   = normalize(s)          │ igual                            │
│ canonicalize(canonicalize) │ Forma canónica es punto fijo     │
│   = canonicalize          │                                  │
│ compile(compile(src))     │ NO idempotente (input cambia)    │
│ Set::insert (con mismo val)│ Insertar duplicado no cambia    │
│ HTTP PUT (mismo recurso)  │ Idempotente por especificación   │
│ HTTP DELETE               │ Borrar lo ya borrado = noop      │
│ format(format(code))      │ Formatear ya formateado no cambia│
└───────────────────────────┴──────────────────────────────────┘
```

### Testing de idempotencia

```rust
use proptest::prelude::*;

// ── Sort es idempotente ──────────────────────────────────────

proptest! {
    #[test]
    fn sort_idempotent(
        v in proptest::collection::vec(any::<i32>(), 0..50)
    ) {
        let mut once = v.clone();
        once.sort();
        
        let mut twice = once.clone();
        twice.sort();
        
        prop_assert_eq!(once, twice);
    }
}

// ── Normalización es idempotente ─────────────────────────────

fn normalize_whitespace(s: &str) -> String {
    s.split_whitespace().collect::<Vec<_>>().join(" ")
}

proptest! {
    #[test]
    fn normalize_idempotent(s in "[ a-z]{0,50}") {
        let once = normalize_whitespace(&s);
        let twice = normalize_whitespace(&once);
        prop_assert_eq!(once, twice);
    }
}

// ── Trim es idempotente ──────────────────────────────────────

proptest! {
    #[test]
    fn trim_idempotent(s in any::<String>()) {
        let once = s.trim().to_string();
        let twice = once.trim().to_string();
        prop_assert_eq!(once, twice);
    }
}

// ── Deduplicación es idempotente ─────────────────────────────

proptest! {
    #[test]
    fn dedup_idempotent(
        v in proptest::collection::vec(0..20i32, 0..30)
    ) {
        let mut once = v;
        once.sort();
        once.dedup();
        
        let mut twice = once.clone();
        twice.sort();
        twice.dedup();
        
        prop_assert_eq!(once, twice);
    }
}

// ── Flatten es idempotente para JSON ─────────────────────────

proptest! {
    #[test]
    fn to_lowercase_idempotent(s in any::<String>()) {
        let once = s.to_lowercase();
        let twice = once.to_lowercase();
        prop_assert_eq!(once, twice);
    }
}
```

### Involutividad: f(f(x)) = x (caso especial)

La involutividad es una forma más fuerte que la idempotencia: aplicar la función dos veces retorna al valor original.

```rust
proptest! {
    #[test]
    fn negate_involutive(x in any::<i32>()) {
        // Evitar overflow con i32::MIN
        prop_assume!(x != i32::MIN);
        prop_assert_eq!(-(-x), x);
    }
    
    #[test]
    fn not_involutive(x in any::<bool>()) {
        prop_assert_eq!(!!x, x);
    }
    
    #[test]
    fn reverse_involutive(
        v in proptest::collection::vec(any::<i32>(), 0..50)
    ) {
        let mut once = v.clone();
        once.reverse();
        once.reverse();
        prop_assert_eq!(v, once);
    }
    
    #[test]
    fn bitwise_not_involutive(x in any::<u32>()) {
        prop_assert_eq!(!!x, x);
    }
    
    #[test]
    fn transpose_involutive(
        rows in 1..5usize,
        cols in 1..5usize
    ) {
        // Generar matriz y transponer dos veces
        let matrix: Vec<Vec<i32>> = (0..rows)
            .map(|r| (0..cols).map(|c| (r * cols + c) as i32).collect())
            .collect();
        
        let transposed = transpose(&matrix);
        let double_transposed = transpose(&transposed);
        
        prop_assert_eq!(matrix, double_transposed);
    }
}

fn transpose(m: &[Vec<i32>]) -> Vec<Vec<i32>> {
    if m.is_empty() || m[0].is_empty() {
        return vec![];
    }
    let rows = m.len();
    let cols = m[0].len();
    (0..cols)
        .map(|c| (0..rows).map(|r| m[r][c]).collect())
        .collect()
}
```

---

## 10. Propiedades de preservación

Las propiedades de preservación verifican que cierta información se conserva entre la entrada y la salida.

### Tipos de preservación

```
  ┌── PRESERVACIÓN DE TAMAÑO ─────────────────────────────────┐
  │  len(f(x)) = len(x)                                       │
  │                                                            │
  │  map:     len(map(v, f)) = len(v)                          │
  │  reverse: len(reverse(v)) = len(v)                         │
  │  sort:    len(sort(v)) = len(v)                            │
  │  shuffle: len(shuffle(v)) = len(v)                         │
  │  NOT:     filter, deduplicate, take, skip                   │
  └────────────────────────────────────────────────────────────┘
  
  ┌── PRESERVACIÓN DE CONTENIDO ──────────────────────────────┐
  │  multiset(f(x)) = multiset(x)                              │
  │                                                            │
  │  sort:    mismos elementos, diferente orden                │
  │  shuffle: mismos elementos, diferente orden                │
  │  reverse: mismos elementos, diferente orden                │
  │  NOT:     map (cambia valores), filter (quita valores)     │
  └────────────────────────────────────────────────────────────┘
  
  ┌── PRESERVACIÓN DE TIPO ───────────────────────────────────┐
  │  type(f(x)) = type(x)                                     │
  │                                                            │
  │  En Rust esto se garantiza por el sistema de tipos.        │
  │  En lenguajes dinámicos sería una propiedad útil.          │
  └────────────────────────────────────────────────────────────┘
  
  ┌── PRESERVACIÓN DE ESTRUCTURA ─────────────────────────────┐
  │  shape(f(x)) = shape(x)                                   │
  │                                                            │
  │  transpose: mismas dimensiones (rows ↔ cols)               │
  │  tree map:  misma forma del árbol                          │
  │  NOT:       insert/delete (cambian la forma)               │
  └────────────────────────────────────────────────────────────┘
  
  ┌── PRESERVACIÓN DE RELACIÓN ───────────────────────────────┐
  │  R(a, b) → R(f(a), f(b))                                  │
  │                                                            │
  │  Monotonía: a ≤ b → f(a) ≤ f(b)                           │
  │  Ejemplo: si a es prefijo de b,                            │
  │           entonces hash(a) NO necesariamente es prefijo    │
  │           de hash(b) ← esto NO se preserva                │
  └────────────────────────────────────────────────────────────┘
```

### Implementación en Rust

```rust
use proptest::prelude::*;
use std::collections::HashMap;

// ── Preservación de contenido (permutación) ──────────────────

fn as_frequency_map(v: &[i32]) -> HashMap<i32, usize> {
    let mut map = HashMap::new();
    for &x in v {
        *map.entry(x).or_insert(0) += 1;
    }
    map
}

proptest! {
    #[test]
    fn sort_preserves_content(
        v in proptest::collection::vec(any::<i32>(), 0..50)
    ) {
        let original_freq = as_frequency_map(&v);
        
        let mut sorted = v;
        sorted.sort();
        let sorted_freq = as_frequency_map(&sorted);
        
        prop_assert_eq!(original_freq, sorted_freq);
    }
    
    // ── Preservación de longitud ─────────────────────────────
    
    #[test]
    fn map_preserves_length(
        v in proptest::collection::vec(any::<i32>(), 0..50)
    ) {
        let mapped: Vec<i32> = v.iter().map(|x| x.wrapping_mul(2)).collect();
        prop_assert_eq!(v.len(), mapped.len());
    }
    
    // ── Preservación de suma ─────────────────────────────────
    
    #[test]
    fn sort_preserves_sum(
        v in proptest::collection::vec(-100..100i32, 0..50)
    ) {
        let original_sum: i32 = v.iter().sum();
        let mut sorted = v;
        sorted.sort();
        let sorted_sum: i32 = sorted.iter().sum();
        prop_assert_eq!(original_sum, sorted_sum);
    }
    
    // ── Preservación de extremos ─────────────────────────────
    
    #[test]
    fn filter_preserves_order(
        v in proptest::collection::vec(any::<i32>(), 0..50),
        threshold in any::<i32>()
    ) {
        let filtered: Vec<i32> = v.iter()
            .filter(|&&x| x > threshold)
            .copied()
            .collect();
        
        // El orden relativo de los elementos se preserva
        // Verificar que filtered es una subsecuencia de v
        let mut v_iter = v.iter();
        for &f in &filtered {
            loop {
                match v_iter.next() {
                    Some(&x) if x == f => break,
                    Some(_) => continue,
                    None => {
                        prop_assert!(
                            false,
                            "filtered no es subsecuencia de original"
                        );
                    }
                }
            }
        }
    }
}
```

### Monotonía: orden preservado

```rust
proptest! {
    #[test]
    fn abs_monotone_for_positives(
        a in 0..i32::MAX,
        b in 0..i32::MAX
    ) {
        // abs es monótona para positivos
        if a <= b {
            prop_assert!(a.abs() <= b.abs());
        }
    }
    
    #[test]
    fn string_len_monotone_over_concat(
        a in "[a-z]{0,20}",
        b in "[a-z]{0,20}"
    ) {
        // Concatenar siempre aumenta o mantiene la longitud
        let ab = format!("{}{}", a, b);
        prop_assert!(ab.len() >= a.len());
        prop_assert!(ab.len() >= b.len());
        prop_assert_eq!(ab.len(), a.len() + b.len());
    }
}
```

---

## 11. Propiedades de inducción y analogía

Estas propiedades relacionan la salida de una función aplicada a datos combinados con la salida aplicada a las partes.

### Homomorfismo: f(x ⊕ y) se relaciona con f(x) ⊗ f(y)

```
  Homomorfismo:
    f(combine(x, y)) = combine'(f(x), f(y))
    
  Ejemplos:
    len(concat(a, b)) = len(a) + len(b)          (⊕ = concat, ⊗ = +)
    sum(concat(a, b)) = sum(a) + sum(b)           (⊕ = concat, ⊗ = +)
    max(concat(a, b)) = max(max(a), max(b))       (⊕ = concat, ⊗ = max)
    sort(concat(a, b)) ≠ concat(sort(a), sort(b)) (NO es homomorfismo)
    log(a * b) = log(a) + log(b)                  (⊕ = *, ⊗ = +)
```

```rust
use proptest::prelude::*;

proptest! {
    // ── len es homomorfismo de concat a + ────────────────────
    
    #[test]
    fn len_homomorphism(
        a in proptest::collection::vec(any::<i32>(), 0..20),
        b in proptest::collection::vec(any::<i32>(), 0..20)
    ) {
        let mut concat = a.clone();
        concat.extend_from_slice(&b);
        
        prop_assert_eq!(concat.len(), a.len() + b.len());
    }
    
    // ── sum es homomorfismo de concat a + ────────────────────
    
    #[test]
    fn sum_homomorphism(
        a in proptest::collection::vec(-100..100i32, 0..20),
        b in proptest::collection::vec(-100..100i32, 0..20)
    ) {
        let sum_a: i32 = a.iter().sum();
        let sum_b: i32 = b.iter().sum();
        
        let mut concat = a;
        concat.extend_from_slice(&b);
        let sum_concat: i32 = concat.iter().sum();
        
        prop_assert_eq!(sum_concat, sum_a + sum_b);
    }
    
    // ── max es homomorfismo de concat a max ──────────────────
    
    #[test]
    fn max_homomorphism(
        a in proptest::collection::vec(any::<i32>(), 1..20),
        b in proptest::collection::vec(any::<i32>(), 1..20)
    ) {
        let max_a = *a.iter().max().unwrap();
        let max_b = *b.iter().max().unwrap();
        
        let mut concat = a;
        concat.extend_from_slice(&b);
        let max_concat = *concat.iter().max().unwrap();
        
        prop_assert_eq!(max_concat, max_a.max(max_b));
    }
}
```

### Propiedad inductiva: dividir y verificar

```rust
proptest! {
    // ── Propiedad inductiva de reverse ───────────────────────
    
    #[test]
    fn reverse_inductive(
        a in proptest::collection::vec(any::<i32>(), 0..20),
        b in proptest::collection::vec(any::<i32>(), 0..20)
    ) {
        // reverse(concat(a, b)) = concat(reverse(b), reverse(a))
        let mut concat = a.clone();
        concat.extend_from_slice(&b);
        concat.reverse();
        
        let mut rev_b = b;
        rev_b.reverse();
        let mut rev_a = a;
        rev_a.reverse();
        rev_b.extend_from_slice(&rev_a);
        
        prop_assert_eq!(concat, rev_b);
    }
}
```

---

## 12. Propiedades negativas: qué NO debe ocurrir

A veces es más fácil expresar lo que una función NO debe hacer que lo que debe hacer.

### Tipos de propiedades negativas

```
  ┌── NUNCA PANIC ─────────────────────────────────────────────┐
  │  f(x) no lanza panic para ningún input x del dominio       │
  │                                                            │
  │  Particularmente útil para:                                │
  │  ├── Parsers (input arbitrario no debe crashear)           │
  │  ├── Deserializadores (bytes corruptos no deben crashear)  │
  │  ├── Validadores (input inválido → error, no panic)        │
  │  └── Operaciones con datos externos (red, archivos)        │
  └────────────────────────────────────────────────────────────┘
  
  ┌── NUNCA PIERDE DATOS ──────────────────────────────────────┐
  │  Datos presentes en la entrada aparecen en la salida       │
  │  (o se registra explícitamente que fueron descartados)      │
  │                                                            │
  │  Ejemplo: migración de esquema no pierde filas             │
  └────────────────────────────────────────────────────────────┘
  
  ┌── NUNCA VIOLA INVARIANTE ──────────────────────────────────┐
  │  La operación no deja la estructura en estado inválido     │
  │                                                            │
  │  Ejemplo: BST sigue siendo BST después de delete           │
  └────────────────────────────────────────────────────────────┘
  
  ┌── NUNCA RETORNA ERROR PARA INPUT VÁLIDO ───────────────────┐
  │  Si el input cumple las precondiciones, la función debe     │
  │  retornar Ok                                               │
  │                                                            │
  │  Ejemplo: parse(format(x)) nunca falla                     │
  └────────────────────────────────────────────────────────────┘
```

### Implementación en Rust

```rust
use proptest::prelude::*;

// ── Nunca panic: parser robusto ──────────────────────────────

fn parse_number(s: &str) -> Result<f64, String> {
    s.trim()
        .parse::<f64>()
        .map_err(|e| format!("parse error: {}", e))
}

proptest! {
    #[test]
    fn parse_never_panics(s in any::<String>()) {
        // No importa qué retorna, no debe hacer panic
        let _ = parse_number(&s);
    }
    
    #[test]
    fn parse_valid_never_fails(
        n in proptest::num::f64::NORMAL
    ) {
        let s = format!("{}", n);
        let result = parse_number(&s);
        prop_assert!(
            result.is_ok(),
            "parse falló para '{}': {:?}", s, result
        );
    }
}

// ── Nunca retorna datos truncados ────────────────────────────

fn split_and_rejoin(s: &str, delimiter: char) -> String {
    s.split(delimiter).collect::<Vec<_>>().join(&delimiter.to_string())
}

proptest! {
    #[test]
    fn split_rejoin_preserves_all_chars(
        s in "[a-z,. ]{0,50}",
        delimiter in prop_oneof![Just(','), Just('.'), Just(' ')]
    ) {
        let result = split_and_rejoin(&s, delimiter);
        
        // No debe perder ningún carácter
        prop_assert_eq!(s, result, "split/rejoin perdió datos");
    }
}

// ── Nunca produce output más grande que el límite ────────────

fn truncate_to(s: &str, max_len: usize) -> &str {
    if s.len() <= max_len {
        s
    } else {
        // Cuidado con cortar en medio de un char multibyte
        let mut end = max_len;
        while !s.is_char_boundary(end) && end > 0 {
            end -= 1;
        }
        &s[..end]
    }
}

proptest! {
    #[test]
    fn truncate_never_exceeds_limit(
        s in any::<String>(),
        max_len in 0..100usize
    ) {
        let truncated = truncate_to(&s, max_len);
        prop_assert!(
            truncated.len() <= max_len,
            "truncado tiene {} bytes, límite era {}",
            truncated.len(), max_len
        );
    }
    
    #[test]
    fn truncate_always_valid_utf8(
        s in any::<String>(),
        max_len in 0..100usize
    ) {
        let truncated = truncate_to(&s, max_len);
        // Si esto compila y no paniquea, es UTF-8 válido
        // (el tipo &str garantiza UTF-8 en Rust)
        prop_assert!(!truncated.is_empty() || s.is_empty() || max_len == 0);
    }
}
```

---

## 13. Hard to compute, easy to verify

Algunas funciones son difíciles de computar pero su resultado es fácil de verificar. Esta asimetría es una fuente natural de propiedades.

### El principio

```
  ┌────────────────────────────────────────────────────────────┐
  │  HARD TO COMPUTE, EASY TO VERIFY                          │
  │                                                            │
  │  Calcular: difícil, complejo, posiblemente lento           │
  │  Verificar: fácil, obvio, rápido                           │
  │                                                            │
  │  Ejemplo: factorización de primos                          │
  │    Calcular: factorize(91) → [7, 13]  (difícil)           │
  │    Verificar: 7 * 13 == 91?            (trivial)           │
  │                                                            │
  │  Ejemplo: ordenamiento                                     │
  │    Calcular: sort(vec) → sorted_vec    (O(n log n))        │
  │    Verificar: ¿está ordenado? ¿mismos elementos?  (O(n))   │
  │                                                            │
  │  Ejemplo: resolver ecuación                                │
  │    Calcular: solve(ax² + bx + c = 0) → raíces  (fórmula)  │
  │    Verificar: a*r² + b*r + c ≈ 0?      (sustituir)        │
  │                                                            │
  │  Ejemplo: regex matching                                   │
  │    Calcular: ¿match "abc" con pattern? (autómata complejo) │
  │    Verificar: ¿el resultado es consistente?  (propiedades) │
  └────────────────────────────────────────────────────────────┘
```

### Implementación en Rust

```rust
use proptest::prelude::*;

// ── Factorización: verificar multiplicando ───────────────────

fn factorize(mut n: u64) -> Vec<u64> {
    let mut factors = Vec::new();
    let mut d = 2;
    while d * d <= n {
        while n % d == 0 {
            factors.push(d);
            n /= d;
        }
        d += 1;
    }
    if n > 1 {
        factors.push(n);
    }
    factors
}

fn is_prime(n: u64) -> bool {
    if n < 2 { return false; }
    if n == 2 { return true; }
    if n % 2 == 0 { return false; }
    let mut i = 3;
    while i * i <= n {
        if n % i == 0 { return false; }
        i += 2;
    }
    true
}

proptest! {
    #[test]
    fn factorize_product_equals_input(n in 2..100_000u64) {
        let factors = factorize(n);
        
        // VERIFICAR: el producto de los factores = n
        let product: u64 = factors.iter().product();
        prop_assert_eq!(product, n, "factores: {:?}", factors);
    }
    
    #[test]
    fn factorize_all_prime(n in 2..100_000u64) {
        let factors = factorize(n);
        
        // VERIFICAR: cada factor es primo
        for &f in &factors {
            prop_assert!(
                is_prime(f),
                "{} no es primo (factores de {}): {:?}",
                f, n, factors
            );
        }
    }
    
    #[test]
    fn factorize_sorted(n in 2..100_000u64) {
        let factors = factorize(n);
        
        // VERIFICAR: factores en orden no decreciente
        for w in factors.windows(2) {
            prop_assert!(w[0] <= w[1]);
        }
    }
}

// ── Raíces de ecuación cuadrática ────────────────────────────

fn solve_quadratic(a: f64, b: f64, c: f64) -> Option<(f64, f64)> {
    let discriminant = b * b - 4.0 * a * c;
    if discriminant < 0.0 || a == 0.0 {
        return None;
    }
    let sqrt_d = discriminant.sqrt();
    let r1 = (-b + sqrt_d) / (2.0 * a);
    let r2 = (-b - sqrt_d) / (2.0 * a);
    Some((r1, r2))
}

proptest! {
    #[test]
    fn quadratic_roots_satisfy_equation(
        a in proptest::num::f64::NORMAL.prop_filter("nonzero", |x| x.abs() > 0.01),
        b in -100.0f64..100.0,
        c in -100.0f64..100.0
    ) {
        if let Some((r1, r2)) = solve_quadratic(a, b, c) {
            // VERIFICAR: sustituir las raíces en la ecuación ≈ 0
            let eval1 = a * r1 * r1 + b * r1 + c;
            let eval2 = a * r2 * r2 + b * r2 + c;
            
            let tolerance = (a.abs() + b.abs() + c.abs()) * 1e-8;
            
            prop_assert!(
                eval1.abs() < tolerance,
                "r1={} no satisface {}x²+{}x+{}=0, eval={}",
                r1, a, b, c, eval1
            );
            prop_assert!(
                eval2.abs() < tolerance,
                "r2={} no satisface {}x²+{}x+{}=0, eval={}",
                r2, a, b, c, eval2
            );
        }
    }
}

// ── Shortest path: verificar que el camino es válido ─────────
// (pseudocódigo conceptual)

// fn shortest_path(graph, start, end) -> Option<Path>
//
// VERIFICAR (fácil):
//   1. El camino empieza en start y termina en end
//   2. Cada arista del camino existe en el grafo
//   3. La longitud reportada = suma de pesos de las aristas
//   4. No existe camino más corto (comparar con BFS/Dijkstra naïve)
```

---

## 14. Propiedades de rendimiento y recursos

Aunque property testing se enfoca en corrección, también se pueden verificar propiedades sobre el uso de recursos.

### Propiedades de complejidad

```rust
use proptest::prelude::*;
use std::time::Instant;

// ── Verificar que no hay complejidad cuadrática accidental ──

proptest! {
    #![proptest_config(ProptestConfig::with_cases(50))]

    #[test]
    fn sort_is_not_quadratic(
        n in prop_oneof![
            Just(100usize),
            Just(1000usize),
            Just(10000usize),
        ]
    ) {
        let v: Vec<i32> = (0..n as i32).rev().collect();
        
        let start = Instant::now();
        let mut sorted = v;
        sorted.sort();
        let elapsed = start.elapsed();
        
        // Para O(n log n), t(10n) / t(n) ≈ 10 * log(10n)/log(n)
        // Para O(n²),      t(10n) / t(n) ≈ 100
        // Verificar que no es absurdamente lento
        // (heurística: 10K elementos no debe tardar más de 100ms)
        if n == 10000 {
            prop_assert!(
                elapsed.as_millis() < 100,
                "sort de {} elementos tardó {:?}, posible O(n²)",
                n, elapsed
            );
        }
    }
}

// ── Verificar que la memoria no crece sin límite ─────────────

proptest! {
    #[test]
    fn vec_capacity_bounded(
        ops in proptest::collection::vec(
            prop_oneof![
                (0..100i32).prop_map(|x| (true, x)),   // push
                Just((false, 0i32)),                     // pop
            ],
            0..200
        )
    ) {
        let mut v = Vec::new();
        let mut max_len = 0usize;
        
        for (is_push, val) in ops {
            if is_push {
                v.push(val);
            } else {
                v.pop();
            }
            max_len = max_len.max(v.len());
        }
        
        // La capacidad no debería ser absurdamente mayor que el máximo
        // necesario (Vec duplica, así que capacity ≤ 2 * max_len + initial)
        prop_assert!(
            v.capacity() <= max_len.next_power_of_two().max(1) * 2,
            "capacity={} parece excesiva para max_len={}",
            v.capacity(), max_len
        );
    }
}
```

---

## 15. Propiedades estadísticas y probabilísticas

Algunas propiedades no se cumplen para cada caso individual sino en agregado estadístico.

### Distribución uniforme de un hash

```rust
use proptest::prelude::*;
use std::collections::hash_map::DefaultHasher;
use std::hash::{Hash, Hasher};

fn simple_hash(key: &str) -> u64 {
    let mut hasher = DefaultHasher::new();
    key.hash(&mut hasher);
    hasher.finish()
}

proptest! {
    #![proptest_config(ProptestConfig::with_cases(1))] // un solo "caso" que genera muchos

    #[test]
    fn hash_distribution_roughly_uniform(_dummy in Just(())) {
        // Generar muchas claves y verificar distribución
        let num_buckets = 16u64;
        let num_keys = 10_000;
        let mut bucket_counts = vec![0u64; num_buckets as usize];
        
        for i in 0..num_keys {
            let key = format!("key_{}", i);
            let bucket = simple_hash(&key) % num_buckets;
            bucket_counts[bucket as usize] += 1;
        }
        
        let expected = num_keys as f64 / num_buckets as f64;
        
        // Cada bucket debería tener ≈ expected ±30% con alta probabilidad
        for (i, &count) in bucket_counts.iter().enumerate() {
            let deviation = (count as f64 - expected).abs() / expected;
            prop_assert!(
                deviation < 0.30,
                "bucket {} tiene {} elementos (esperado ≈{}), desviación {:.1}%",
                i, count, expected, deviation * 100.0
            );
        }
    }
}
```

### Propiedades de efecto avalancha (hashing)

```rust
proptest! {
    #[test]
    fn hash_avalanche_one_bit_flip(key in "[a-z]{5,20}") {
        let hash1 = simple_hash(&key);
        
        // Cambiar un byte
        let mut bytes = key.into_bytes();
        if !bytes.is_empty() {
            bytes[0] ^= 1; // flip 1 bit
            let modified = String::from_utf8_lossy(&bytes).to_string();
            let hash2 = simple_hash(&modified);
            
            // Los hashes deberían ser "muy diferentes"
            // (al menos varios bits de diferencia)
            let diff_bits = (hash1 ^ hash2).count_ones();
            prop_assert!(
                diff_bits >= 10, // al menos 10 de 64 bits cambiaron
                "solo {} bits cambiaron al flipear 1 bit en input",
                diff_bits
            );
        }
    }
}
```

---

## 16. El proceso de descubrimiento de propiedades

Un proceso sistemático para encontrar propiedades cuando no son obvias.

### El método de las 7 preguntas

```
  Para cualquier función f(x) → y, pregúntate:
  
  ┌─────────────────────────────────────────────────────────────┐
  │                                                             │
  │  1. ¿HAY UNA INVERSA?                                      │
  │     Si decode(encode(x)) = x → ROUND-TRIP                  │
  │                                                             │
  │  2. ¿QUÉ SE PRESERVA?                                      │
  │     ¿Longitud? ¿Contenido? ¿Suma? ¿Orden? → PRESERVACIÓN  │
  │                                                             │
  │  3. ¿QUÉ SIEMPRE ES VERDAD SOBRE Y?                        │
  │     ¿Siempre ordenado? ¿Siempre positivo? → INVARIANTE     │
  │                                                             │
  │  4. ¿QUÉ PASA SI APLICO f DOS VECES?                       │
  │     f(f(x)) = f(x) → IDEMPOTENCIA                          │
  │     f(f(x)) = x     → INVOLUTIVIDAD                        │
  │                                                             │
  │  5. ¿HAY OTRA FORMA DE CALCULAR LO MISMO?                  │
  │     std::sort, HashMap, fórmula cerrada → ORACLE            │
  │                                                             │
  │  6. ¿QUÉ LEYES ALGEBRAICAS APLICAN?                        │
  │     f(a,b) = f(b,a)? f(f(a,b),c) = f(a,f(b,c))? → ÁLGEBRA│
  │                                                             │
  │  7. ¿ES FÁCIL DE VERIFICAR AUNQUE SEA DIFÍCIL DE CALCULAR? │
  │     Factores: multiplicar. Raíces: sustituir. → VERIFY      │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

### Ejemplo: aplicar las 7 preguntas a `HashMap::insert`

```
  Función: map.insert(key, value)
  
  1. ¿Inversa?
     Parcial: map.remove(key) deshace insert (si no existía antes)
     → Propiedad: insert(k,v) seguido de remove(k) restaura estado previo
  
  2. ¿Qué se preserva?
     → len aumenta en 0 o 1
     → Otras claves no cambian
     → Propiedad: después de insert, get(otra_clave) no cambia
  
  3. ¿Invariante de salida?
     → map.get(key) == Some(value) después de insert
     → len > 0 después de insert
     → Propiedad: insert garantiza contains_key
  
  4. ¿Idempotente?
     → insert(k,v) dos veces = insert(k,v) una vez (mismo resultado)
     → Propiedad: insertar la misma clave/valor dos veces no cambia el map
  
  5. ¿Oracle?
     → BTreeMap tiene la misma API → comparar resultados
  
  6. ¿Leyes algebraicas?
     → insert(k,v1) seguido de insert(k,v2) = insert(k,v2) solamente
       (última escritura gana)
     → Propiedad: el orden de inserts de DIFERENTES claves no importa
  
  7. ¿Fácil de verificar?
     → Después de N inserts, verificar que contains_key es O(N*1)
```

### Ejemplo: aplicar las 7 preguntas a una función `compress`

```
  Función: compress(data: &[u8]) → Vec<u8>
  
  1. ¿Inversa? SÍ → decompress(compress(data)) = data ← ROUND-TRIP
  
  2. ¿Qué se preserva?
     → Contenido (a través de decompress)
     → Propiedad: los bytes originales son recuperables
  
  3. ¿Invariante de salida?
     → Output tiene header válido
     → Output es decodificable
     → Propiedad: compress siempre produce output válido
  
  4. ¿Idempotente?
     → compress(compress(data)) ≠ compress(data) generalmente
       (comprimir datos ya comprimidos a menudo los agranda)
     → Propiedad: decompress(compress(data)) = data (no idempotente,
       pero el round-trip sí funciona)
  
  5. ¿Oracle?
     → Implementación de referencia (zlib, etc.)
     → Propiedad: compress(data) produce output que zlib puede decodificar
  
  6. ¿Leyes algebraicas?
     → No muchas (la compresión no es una operación algebraica típica)
     → Pero: compress([]) = algo fijo (identidad para input vacío)
  
  7. ¿Fácil de verificar?
     → decompress(resultado) = data original ← ya cubierto por round-trip
     → len(resultado) > 0 para input no vacío
```

---

## 17. Patrones por dominio: qué propiedades buscar según el problema

### Guía por tipo de función

```
┌─────────────────────────┬─────────────────────────────────────────┐
│ Tipo de función         │ Propiedades a buscar                    │
├─────────────────────────┼─────────────────────────────────────────┤
│ Serialización           │ • Round-trip (deserialize ∘ serialize = id)
│ (JSON, YAML, binario)   │ • Formato válido (output parseable)     │
│                         │ • Preservación de tipos                 │
│                         │ • Determinismo (misma entrada → misma   │
│                         │   salida)                               │
├─────────────────────────┼─────────────────────────────────────────┤
│ Ordenamiento            │ • Resultado ordenado                    │
│                         │ • Preservación de elementos (permutación)│
│                         │ • Preservación de longitud              │
│                         │ • Idempotencia                          │
│                         │ • Estabilidad (para sort estable)       │
│                         │ • Min primero, max último               │
├─────────────────────────┼─────────────────────────────────────────┤
│ Colecciones (Map, Set)  │ • Insert → contains                    │
│                         │ • Delete → !contains                    │
│                         │ • Idempotencia de insert                │
│                         │ • Conmutatividad del orden de inserts   │
│                         │ • Oracle (vs std::collections)          │
│                         │ • Invariantes internas                  │
├─────────────────────────┼─────────────────────────────────────────┤
│ Parser / Formatter      │ • Round-trip (parse ∘ format = id)      │
│                         │ • Nunca panic en input arbitrario       │
│                         │ • Error en input inválido               │
│                         │ • Formato canónico (format ∘ parse ∘    │
│                         │   format = format)                      │
├─────────────────────────┼─────────────────────────────────────────┤
│ Criptografía            │ • Round-trip (decrypt ∘ encrypt = id)   │
│                         │ • Efecto avalancha                      │
│                         │ • Ciphertext ≠ plaintext                │
│                         │ • Diferentes keys → diferentes outputs  │
│                         │ • Longitud del output predecible        │
├─────────────────────────┼─────────────────────────────────────────┤
│ Compresión              │ • Round-trip (decompress ∘ compress = id)│
│                         │ • Output no mayor que input + overhead  │
│                         │ • Datos aleatorios: compresión ≈ 0      │
│                         │ • Datos repetitivos: compresión alta    │
├─────────────────────────┼─────────────────────────────────────────┤
│ Búsqueda / Filtrado     │ • Todos los resultados cumplen criterio │
│                         │ • No se perdió ningún resultado         │
│                         │   (completeness)                        │
│                         │ • Orden preservado                      │
│                         │ • Subconjunto del input                 │
├─────────────────────────┼─────────────────────────────────────────┤
│ Aritmética numérica     │ • Conmutatividad, asociatividad         │
│                         │ • Identidad (0 para +, 1 para *)        │
│                         │ • Distributividad                       │
│                         │ • Inversos (a + (-a) = 0)               │
│                         │ • Monotonía                             │
│                         │ • Acotamiento (resultado en rango)      │
├─────────────────────────┼─────────────────────────────────────────┤
│ Máquina de estado       │ • Invariantes después de cada transición│
│                         │ • Oracle (vs modelo de referencia)       │
│                         │ • Transiciones imposibles rechazan      │
│                         │ • Estado final alcanzable               │
├─────────────────────────┼─────────────────────────────────────────┤
│ Concurrencia            │ • Linearizabilidad                      │
│                         │ • Resultado idéntico a ejecución serial │
│                         │ • Sin deadlocks (termina siempre)       │
│                         │ • Sin data races (resultados consistentes)│
├─────────────────────────┼─────────────────────────────────────────┤
│ Generación de código    │ • Output compila sin errores            │
│ (codegen, templates)    │ • Output produce resultados correctos   │
│                         │ • Output no depende del orden de input  │
│                         │ • Idempotencia de la generación         │
└─────────────────────────┴─────────────────────────────────────────┘
```

---

## 18. De la especificación a las propiedades

Cada requisito de una especificación puede convertirse en una o más propiedades testeables.

### Proceso de traducción

```
  Especificación (lenguaje natural)
         │
         ▼
  Identificar SUJETO (qué función)
         │
         ▼
  Identificar CUANTIFICADOR (∀ input, ∃ output)
         │
         ▼
  Identificar PREDICADO (qué debe cumplirse)
         │
         ▼
  Traducir a CÓDIGO proptest
```

### Ejemplo: especificación de un validador de contraseñas

```
  ESPECIFICACIÓN:
  
  R1: La contraseña debe tener al menos 8 caracteres
  R2: La contraseña debe tener al menos 1 mayúscula
  R3: La contraseña debe tener al menos 1 minúscula
  R4: La contraseña debe tener al menos 1 dígito
  R5: La contraseña no debe contener espacios
  R6: Contraseñas que cumplen R1-R5 deben ser aceptadas
  R7: Contraseñas que violan alguna de R1-R5 deben ser rechazadas
```

```rust
use proptest::prelude::*;

fn validate_password(pw: &str) -> Result<(), Vec<String>> {
    let mut errors = Vec::new();
    
    if pw.len() < 8 {
        errors.push("too short".to_string());
    }
    if !pw.chars().any(|c| c.is_uppercase()) {
        errors.push("no uppercase".to_string());
    }
    if !pw.chars().any(|c| c.is_lowercase()) {
        errors.push("no lowercase".to_string());
    }
    if !pw.chars().any(|c| c.is_ascii_digit()) {
        errors.push("no digit".to_string());
    }
    if pw.contains(' ') {
        errors.push("contains space".to_string());
    }
    
    if errors.is_empty() {
        Ok(())
    } else {
        Err(errors)
    }
}

// Estrategia: contraseñas que CUMPLEN todos los requisitos
fn valid_password_strategy() -> impl Strategy<Value = String> {
    (
        "[A-Z]",                    // al menos 1 mayúscula
        "[a-z]{3,10}",              // al menos 1 minúscula (varias)
        "[0-9]{2,4}",               // al menos 1 dígito (varios)
        "[!@#$%^&*]{0,3}",          // caracteres especiales opcionales
    )
        .prop_map(|(upper, lower, digits, special)| {
            format!("{}{}{}{}", upper, lower, digits, special)
        })
        .prop_filter("mínimo 8 chars", |s| s.len() >= 8)
}

proptest! {
    // R6: Contraseñas válidas deben ser aceptadas
    #[test]
    fn valid_passwords_accepted(pw in valid_password_strategy()) {
        let result = validate_password(&pw);
        prop_assert!(
            result.is_ok(),
            "contraseña válida rechazada: '{}', errores: {:?}",
            pw, result.err()
        );
    }
    
    // R1: Demasiado corta → rechazada
    #[test]
    fn short_passwords_rejected(pw in "[A-Za-z0-9]{1,7}") {
        let result = validate_password(&pw);
        prop_assert!(result.is_err());
        let errors = result.unwrap_err();
        prop_assert!(errors.iter().any(|e| e.contains("too short")));
    }
    
    // R2: Sin mayúsculas → rechazada
    #[test]
    fn no_uppercase_rejected(pw in "[a-z0-9]{8,20}") {
        let result = validate_password(&pw);
        prop_assert!(result.is_err());
        let errors = result.unwrap_err();
        prop_assert!(errors.iter().any(|e| e.contains("uppercase")));
    }
    
    // R3: Sin minúsculas → rechazada
    #[test]
    fn no_lowercase_rejected(pw in "[A-Z0-9]{8,20}") {
        let result = validate_password(&pw);
        prop_assert!(result.is_err());
        let errors = result.unwrap_err();
        prop_assert!(errors.iter().any(|e| e.contains("lowercase")));
    }
    
    // R4: Sin dígitos → rechazada
    #[test]
    fn no_digits_rejected(pw in "[A-Za-z]{8,20}") {
        let result = validate_password(&pw);
        prop_assert!(result.is_err());
        let errors = result.unwrap_err();
        prop_assert!(errors.iter().any(|e| e.contains("digit")));
    }
    
    // R5: Con espacios → rechazada
    #[test]
    fn spaces_rejected(
        pw in ("[A-Z][a-z]{2,5} [0-9]{2,4}")
            .prop_filter("mínimo 8", |s| s.len() >= 8)
    ) {
        let result = validate_password(&pw);
        prop_assert!(result.is_err());
        let errors = result.unwrap_err();
        prop_assert!(errors.iter().any(|e| e.contains("space")));
    }
    
    // Propiedad meta: nunca panic con input arbitrario
    #[test]
    fn validate_never_panics(pw in any::<String>()) {
        let _ = validate_password(&pw);
    }
    
    // Propiedad: resultado es consistente (determinístico)
    #[test]
    fn validate_deterministic(pw in any::<String>()) {
        let r1 = validate_password(&pw);
        let r2 = validate_password(&pw);
        prop_assert_eq!(r1, r2);
    }
}
```

---

## 19. Propiedades compuestas: combinar varias propiedades débiles

Una propiedad débil individual puede ser insuficiente, pero la combinación de varias propiedades débiles puede ser tan fuerte como una especificación completa.

### El problema de las propiedades débiles individuales

```
  Propiedad: "sort preserva la longitud"
  
  Funciones que pasan:
    ✓ sort correcto
    ✓ función identidad (retorna el input sin cambio)
    ✓ función que pone todos los elementos a 0
    ✓ función que rota los elementos
    
  → MUY débil sola
  
  
  Propiedad: "sort produce resultado ordenado"
  
  Funciones que pasan:
    ✓ sort correcto
    ✓ función que retorna [1, 2, 3, ..., n]
    ✓ función que retorna vec vacío (trivialmente ordenado)
    ✓ función que retorna [min(v); n]
    
  → Débil sola también
  
  
  COMBINACIÓN: "preserva longitud" + "preserva elementos" + "resultado ordenado"
  
  Funciones que pasan TODAS:
    ✓ sort correcto
    ✗ identidad (no siempre ordenada)
    ✗ todos a 0 (no preserva elementos)
    ✗ [1,2,...,n] (no preserva elementos)
    
  → FUERTE: SOLO sort correcto las pasa todas
```

### Diseño de conjuntos de propiedades

```
  ESTRATEGIA: empezar con propiedades débiles y fortalecer
  
  Paso 1: Escribir la propiedad más obvia
          "sort preserva longitud" — débil pero fácil
  
  Paso 2: Preguntarse "¿qué funciones incorrectas pasan?"
          → f(v) = vec![0; v.len()] pasa
  
  Paso 3: Agregar propiedad que descarta esas funciones
          + "sort preserva elementos" — ahora [0; n] no pasa
  
  Paso 4: Preguntarse de nuevo
          → f(v) = v (identidad) pasa ambas
  
  Paso 5: Agregar propiedad final
          + "resultado ordenado" — ahora identidad no pasa
  
  Paso 6: Verificar que la combinación es suficiente
          ¿Existe una función NO-sort que pase las 3? → No ✓
```

### Ejemplo: propiedades compuestas para `filter`

```rust
use proptest::prelude::*;

fn my_filter(v: &[i32], predicate: impl Fn(&i32) -> bool) -> Vec<i32> {
    v.iter().filter(|x| predicate(x)).copied().collect()
}

proptest! {
    #[test]
    fn filter_properties_combined(
        v in proptest::collection::vec(any::<i32>(), 0..50),
        threshold in any::<i32>()
    ) {
        let predicate = |x: &i32| *x > threshold;
        let filtered = my_filter(&v, predicate);
        
        // Propiedad débil 1: todos los resultados cumplen el predicado
        // (falla si filter incluye falsos positivos)
        for &x in &filtered {
            prop_assert!(predicate(&x), "falso positivo: {}", x);
        }
        
        // Propiedad débil 2: no se perdió ningún elemento que cumple
        // (falla si filter tiene falsos negativos)
        let expected_count = v.iter().filter(|x| predicate(x)).count();
        prop_assert_eq!(
            filtered.len(), expected_count,
            "se perdieron elementos"
        );
        
        // Propiedad débil 3: el orden relativo se preserva
        // (falla si filter reordena)
        if filtered.len() >= 2 {
            // Encontrar posiciones en el original
            let mut last_pos = 0;
            for &f in &filtered {
                let pos = v[last_pos..].iter()
                    .position(|&x| x == f)
                    .map(|p| p + last_pos);
                prop_assert!(pos.is_some(), "elemento no encontrado");
                last_pos = pos.unwrap() + 1;
            }
        }
        
        // Propiedad débil 4: es subconjunto del original
        prop_assert!(filtered.len() <= v.len());
        
        // JUNTAS: estas 4 propiedades caracterizan filter completamente.
        // Una función que pasa las 4 es un filter correcto.
    }
}
```

### La regla del "adversario"

Para evaluar si tus propiedades son suficientes, juega al "adversario":

```
  JUEGO DEL ADVERSARIO:
  
  1. Tú propones un conjunto de propiedades
  
  2. El adversario intenta implementar una función INCORRECTA
     que pase TODAS tus propiedades
  
  3. Si el adversario lo logra, necesitas más propiedades
  
  4. Si no puede, tu conjunto es suficiente
  
  
  Ejemplo para sort:
  
  Propiedades: {ordenado, misma longitud, mismos elementos}
  
  Adversario intenta:
    f(v) = v                → pasa longitud y elementos, falla en ordenado
    f(v) = [0; v.len()]     → pasa longitud y ordenado, falla en elementos
    f(v) = sort(v)[..v.len()-1] → falla en longitud
    f(v) = sort(v) + [0]    → falla en longitud
    f(v) = sort(v)           → pasa todo ← ES CORRECTO
    
  El adversario no puede hacer trampa → propiedades suficientes ✓
```

---

## 20. Trampas al diseñar propiedades

### Trampa 1: reimplementar la función en la propiedad

```rust
// ❌ TRAMPA: la propiedad ES la implementación
proptest! {
    #[test]
    fn test_malo(a in any::<i32>(), b in any::<i32>()) {
        let expected = a.wrapping_add(b); // Reimplementando add
        let actual = my_add(a, b);
        prop_assert_eq!(actual, expected);
        // Si my_add tiene el mismo bug que wrapping_add,
        // este test nunca lo encontrará
    }
}

// ✅ CORRECTO: usar propiedades que no reimplementan
proptest! {
    #[test]
    fn test_bueno(a in any::<i32>(), b in any::<i32>()) {
        // Conmutatividad: no reimplementa, verifica una LEY
        prop_assert_eq!(
            my_add(a, b),
            my_add(b, a)
        );
    }
}
```

### Trampa 2: propiedades tautológicas

```rust
// ❌ TRAMPA: siempre verdad por definición
proptest! {
    #[test]
    fn tautologia(x in 0..100i32) {
        prop_assert!(x >= 0 && x < 100);
        // Esto solo verifica la ESTRATEGIA, no el CÓDIGO
    }
}

// ✅ CORRECTO: verificar algo sobre el código, no sobre la estrategia
proptest! {
    #[test]
    fn no_tautologia(x in 0..100i32) {
        let result = process(x);  // función a testear
        prop_assert!(result >= 0); // propiedad del RESULTADO
    }
}
```

### Trampa 3: propiedades demasiado débiles

```rust
// ❌ TRAMPA: tan débil que es inútil
proptest! {
    #[test]
    fn demasiado_debil(v in proptest::collection::vec(any::<i32>(), 0..20)) {
        let sorted = my_sort(&v);
        prop_assert!(sorted.len() <= v.len() + 1000);
        // Esta propiedad permite que sort agregue hasta 1000 elementos basura
    }
}

// ✅ CORRECTO: ajustar la propiedad al comportamiento exacto esperado
proptest! {
    #[test]
    fn preciso(v in proptest::collection::vec(any::<i32>(), 0..20)) {
        let sorted = my_sort(&v);
        prop_assert_eq!(sorted.len(), v.len());
    }
}
```

### Trampa 4: dominio de entrada incompleto

```rust
// ❌ TRAMPA: solo testear con datos "bonitos"
proptest! {
    #[test]
    fn dominio_restringido(s in "[a-z]{1,10}") {
        let parsed = my_parse(&s);
        prop_assert!(parsed.is_ok());
        // Nunca testea: strings vacíos, Unicode, muy largos, 
        // caracteres especiales, null bytes...
    }
}

// ✅ CORRECTO: incluir edge cases en la estrategia
proptest! {
    #[test]
    fn dominio_completo(s in any::<String>()) {
        let result = my_parse(&s);
        // No asumimos Ok o Err, solo que no hace panic
        match result {
            Ok(parsed) => {
                // Si parsea exitosamente, verificar propiedades
                prop_assert!(!parsed.is_empty());
            }
            Err(_) => {
                // Error está bien para input inválido
            }
        }
    }
}
```

### Trampa 5: asumir flotantes se comportan como enteros

```rust
// ❌ TRAMPA: asumir asociatividad exacta con floats
proptest! {
    #[test]
    fn float_associative_bad(
        a in proptest::num::f64::NORMAL,
        b in proptest::num::f64::NORMAL,
        c in proptest::num::f64::NORMAL
    ) {
        prop_assert_eq!((a + b) + c, a + (b + c));
        // FALLA: punto flotante NO es asociativo
        // (1e20 + 1e-20) + 1e-20 ≠ 1e20 + (1e-20 + 1e-20)
    }
}

// ✅ CORRECTO: usar tolerancia
proptest! {
    #[test]
    fn float_associative_good(
        a in -1000.0f64..1000.0,
        b in -1000.0f64..1000.0,
        c in -1000.0f64..1000.0
    ) {
        let lhs = (a + b) + c;
        let rhs = a + (b + c);
        let diff = (lhs - rhs).abs();
        let scale = a.abs() + b.abs() + c.abs() + 1.0;
        
        // Tolerancia relativa
        prop_assert!(
            diff / scale < 1e-10,
            "({} + {}) + {} = {}, {} + ({} + {}) = {}, diff = {}",
            a, b, c, lhs, a, b, c, rhs, diff
        );
    }
}
```

### Trampa 6: la propiedad no testea lo que crees

```rust
// ❌ TRAMPA: la propiedad no verifica lo importante
proptest! {
    #[test]
    fn encryption_bad_test(
        data in proptest::collection::vec(any::<u8>(), 1..100),
        key in proptest::collection::vec(any::<u8>(), 32)
    ) {
        let encrypted = encrypt(&data, &key);
        prop_assert!(!encrypted.is_empty()); // ← ¿y?
        // No verifica: 
        //   - que decrypt funcione
        //   - que ciphertext ≠ plaintext
        //   - que diferentes keys produzcan diferente output
    }
}

// ✅ CORRECTO: propiedades que importan
proptest! {
    #[test]
    fn encryption_roundtrip(
        data in proptest::collection::vec(any::<u8>(), 1..100),
        key in proptest::collection::vec(any::<u8>(), 32)
    ) {
        let encrypted = encrypt(&data, &key);
        let decrypted = decrypt(&encrypted, &key).unwrap();
        prop_assert_eq!(data, decrypted); // ← round-trip, verifica MUCHO más
    }
    
    #[test]
    fn encryption_changes_data(
        data in proptest::collection::vec(any::<u8>(), 16..100),
        key in proptest::collection::vec(any::<u8>(), 32)
    ) {
        let encrypted = encrypt(&data, &key);
        prop_assert_ne!(data, encrypted); // ← ciphertext ≠ plaintext
    }
}
```

---

## 21. Comparación con C y Go

### Cómo se diseñan propiedades en cada lenguaje

```c
// ══════════════════════════════════════════════════════════════
// C: diseñar propiedades con theft
// ══════════════════════════════════════════════════════════════

// En C, el proceso es el mismo conceptualmente,
// pero la implementación es mucho más verbosa.

#include <theft.h>
#include <string.h>
#include <stdlib.h>

// Propiedad: sort preserva longitud
static enum theft_trial_res
prop_sort_preserves_length(struct theft *t, void *arg1) {
    struct int_array *arr = (struct int_array *)arg1;
    
    int *sorted = malloc(arr->len * sizeof(int));
    memcpy(sorted, arr->data, arr->len * sizeof(int));
    qsort(sorted, arr->len, sizeof(int), int_cmp);
    
    // En C, "preservar longitud" es implícito (usamos el mismo len)
    // La propiedad aquí sería verificar que no modifica len
    
    // Propiedad: resultado está ordenado
    for (size_t i = 1; i < arr->len; i++) {
        if (sorted[i] < sorted[i-1]) {
            free(sorted);
            return THEFT_TRIAL_FAIL;
        }
    }
    
    free(sorted);
    return THEFT_TRIAL_PASS;
}

// En C:
//   ❌ Sin closures → no puedes parametrizar propiedades fácilmente
//   ❌ Sin genéricos → cada tipo necesita su propia infraestructura
//   ❌ Sin Pattern matching → más código boilerplate
//   ❌ Sin traits → no hay Arbitrary automático
//   ✅ El PENSAMIENTO sobre propiedades es el mismo
```

```go
// ══════════════════════════════════════════════════════════════
// Go: diseñar propiedades con testing/quick o gopter
// ══════════════════════════════════════════════════════════════

package sorting

import (
    "sort"
    "testing"
    "testing/quick"
)

// Propiedad: sort preserva longitud
func TestSortPreservesLength(t *testing.T) {
    f := func(input []int) bool {
        original_len := len(input)
        sorted := make([]int, len(input))
        copy(sorted, input)
        sort.Ints(sorted)
        return len(sorted) == original_len
    }
    if err := quick.Check(f, nil); err != nil {
        t.Error(err)
    }
}

// Propiedad: sort produce resultado ordenado
func TestSortResultSorted(t *testing.T) {
    f := func(input []int) bool {
        sorted := make([]int, len(input))
        copy(sorted, input)
        sort.Ints(sorted)
        return sort.IntsAreSorted(sorted)
    }
    if err := quick.Check(f, nil); err != nil {
        t.Error(err)
    }
}

// Propiedad: sort preserva elementos
func TestSortPreservesElements(t *testing.T) {
    f := func(input []int) bool {
        sorted := make([]int, len(input))
        copy(sorted, input)
        sort.Ints(sorted)
        
        // Verificar mismo multiconjunto
        original := make([]int, len(input))
        copy(original, input)
        sort.Ints(original)
        
        for i := range sorted {
            if sorted[i] != original[i] {
                return false
            }
        }
        return true
    }
    if err := quick.Check(f, nil); err != nil {
        t.Error(err)
    }
}

// Go:
//   ✅ testing/quick es simple para propiedades básicas
//   ❌ Sin generadores composables (necesitas gopter)
//   ❌ Shrinking muy básico en testing/quick
//   ❌ Sin regex strategies
//   ✅ El PENSAMIENTO sobre propiedades es el mismo
//   ✅ Genéricos (desde Go 1.18) ayudan algo
```

### Tabla comparativa del diseño de propiedades

```
┌────────────────────┬──────────────┬──────────────┬───────────────┐
│ Aspecto            │ C (theft)    │ Go (quick)   │ Rust (proptest)│
├────────────────────┼──────────────┼──────────────┼───────────────┤
│ Diseño mental      │ Igual        │ Igual        │ Igual          │
│ (propiedades)      │              │              │                │
│                    │              │              │                │
│ Expresividad       │ Baja         │ Media        │ Alta           │
│ (implementación)   │ (verbose)    │              │ (macros,       │
│                    │              │              │  composición)  │
│                    │              │              │                │
│ Round-trip         │ Manual       │ Manual       │ prop_assert_eq!│
│                    │              │              │ + any::<T>()   │
│                    │              │              │                │
│ Invariantes        │ if/return    │ return bool  │ prop_assert!   │
│                    │              │              │                │
│ Oracle             │ strcmp/memcmp│ reflect.     │ assert_eq! +   │
│                    │              │ DeepEqual    │ PartialEq      │
│                    │              │              │                │
│ Propiedades        │ Funciones    │ Funciones    │ Closures en    │
│ parametrizadas     │ separadas    │ separadas    │ proptest! {}   │
│                    │              │              │                │
│ Composición de     │ No           │ No (quick)   │ Sí (combina-   │
│ propiedades        │              │ Sí (gopter)  │ dores)         │
│                    │              │              │                │
│ Generación de      │ Muy manual   │ Básica/      │ Rica y         │
│ inputs para        │              │ manual       │ composable     │
│ propiedades        │              │              │                │
└────────────────────┴──────────────┴──────────────┴───────────────┘
```

---

## 22. Errores comunes al diseñar propiedades

### Error 1: propiedad que no testea el código sino la estrategia

```rust
// ❌ Solo verifica que proptest genera bien
proptest! {
    #[test]
    fn test_la_estrategia(x in 1..100i32) {
        prop_assert!(x >= 1);    // verifica la ESTRATEGIA, no tu código
        prop_assert!(x < 100);   // verifica la ESTRATEGIA
    }
}

// ✅ Verifica una propiedad del CÓDIGO bajo test
proptest! {
    #[test]
    fn test_el_codigo(x in 1..100i32) {
        let result = my_function(x);
        prop_assert!(result > 0); // verifica el CÓDIGO
    }
}
```

### Error 2: propiedad que duplica la implementación

```rust
// ❌ Es un oracle donde el oracle es una copia del código
fn celsius_to_fahrenheit(c: f64) -> f64 {
    c * 9.0 / 5.0 + 32.0
}

proptest! {
    #[test]
    fn test_duplicado(c in -100.0f64..100.0) {
        let expected = c * 9.0 / 5.0 + 32.0;  // ← COPIA de la implementación
        let actual = celsius_to_fahrenheit(c);
        prop_assert_eq!(actual, expected);
        // Si la fórmula está mal, ¡ambas están mal de la misma forma!
    }
}

// ✅ Usar propiedades que no replican la fórmula
proptest! {
    #[test]
    fn test_con_propiedades(c in -100.0f64..100.0) {
        let f = celsius_to_fahrenheit(c);
        
        // Puntos fijos conocidos (sin replicar fórmula)
        // 0°C = 32°F
        if c == 0.0 {
            prop_assert_eq!(f, 32.0);
        }
        
        // Monotonía: más caliente en C → más caliente en F
        let f2 = celsius_to_fahrenheit(c + 1.0);
        prop_assert!(f2 > f, "no es monótona creciente");
        
        // Linealidad: la diferencia es constante
        let diff = f2 - f;
        prop_assert!((diff - 1.8).abs() < 1e-10, "no es lineal");
        
        // Round-trip con fahrenheit_to_celsius (si existe)
        // let recovered = fahrenheit_to_celsius(f);
        // prop_assert!((recovered - c).abs() < 1e-10);
    }
}
```

### Error 3: propiedad correcta pero dominio insuficiente

```rust
// ❌ Solo testea con enteros positivos pequeños
proptest! {
    #[test]
    fn test_dominio_limitado(x in 1..10i32) {
        prop_assert!(my_abs(x) >= 0);
        // Nunca testea: 0, negativos, i32::MIN, valores grandes
    }
}

// ✅ Incluir todos los subdominios relevantes
proptest! {
    #[test]
    fn test_dominio_completo(x in any::<i32>()) {
        if x == i32::MIN {
            // Caso especial: abs(MIN) overflow
            // Verificar que no hace panic o que retorna error
            // (depende de la especificación)
        } else {
            let result = my_abs(x);
            prop_assert!(result >= 0);
            prop_assert_eq!(result, if x >= 0 { x } else { -x });
        }
    }
}
```

### Error 4: confundir propiedades necesarias con suficientes

```rust
// ❌ Propiedad necesaria (debe pasar) pero NO suficiente
//    (no descarta implementaciones incorrectas)
proptest! {
    #[test]
    fn necesaria_no_suficiente(
        v in proptest::collection::vec(any::<i32>(), 0..20)
    ) {
        let result = my_sort(&v);
        prop_assert_eq!(result.len(), v.len());
        // f(v) = v pasa esta propiedad, pero no es sort
    }
}

// ✅ Conjunto de propiedades que es NECESARIO Y SUFICIENTE
proptest! {
    #[test]
    fn necesaria_y_suficiente(
        v in proptest::collection::vec(any::<i32>(), 0..20)
    ) {
        let result = my_sort(&v);
        
        // Necesaria 1 + Necesaria 2 + Necesaria 3 = Suficiente
        // 1. Misma longitud
        prop_assert_eq!(result.len(), v.len());
        
        // 2. Mismos elementos (permutación)
        let mut expected = v;
        expected.sort();
        let mut actual = result.clone();
        actual.sort();
        prop_assert_eq!(actual, expected);
        
        // 3. Ordenado
        for w in result.windows(2) {
            prop_assert!(w[0] <= w[1]);
        }
    }
}
```

### Error 5: propiedades que solo pasan por coincidencia del rango

```rust
// ❌ Pasa porque el rango es demasiado pequeño para exhibir el bug
proptest! {
    #[test]
    fn rango_insuficiente(x in 0..10u32) {
        // Si my_function tiene un bug para x > 1000, nunca lo encontramos
        let result = my_function(x);
        prop_assert!(result < 100);
    }
}

// ✅ Usar el rango completo del tipo
proptest! {
    #[test]
    fn rango_completo(x in any::<u32>()) {
        let result = my_function(x);
        // Propiedad que debe cumplirse para TODO el rango
        prop_assert!(result <= u32::MAX);
    }
}
```

### Error 6: ignorar la relación entre inputs

```rust
// ❌ Inputs independientes cuando deberían estar relacionados
proptest! {
    #[test]
    fn test_independientes(
        v in proptest::collection::vec(any::<i32>(), 1..20),
        idx in any::<usize>()
    ) {
        // idx puede ser mayor que v.len() → panic
        let _ = v[idx]; // ¡PANIC!
    }
}

// ✅ Inputs dependientes con prop_flat_map
proptest! {
    #[test]
    fn test_dependientes(
        (v, idx) in proptest::collection::vec(any::<i32>(), 1..20)
            .prop_flat_map(|v| {
                let len = v.len();
                (Just(v), 0..len)
            })
    ) {
        let _ = v[idx]; // Siempre válido
        prop_assert!(idx < v.len());
    }
}
```

---

## 23. Ejemplo completo: propiedades para un sistema de permisos

### Sistema de permisos tipo RBAC (Role-Based Access Control)

```rust
// src/lib.rs

use std::collections::{HashMap, HashSet};

/// Permisos individuales
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Permission {
    Read,
    Write,
    Delete,
    Admin,
}

/// Un rol es un conjunto nombrado de permisos
#[derive(Debug, Clone)]
pub struct Role {
    pub name: String,
    pub permissions: HashSet<Permission>,
}

/// Sistema de control de acceso
#[derive(Debug, Clone)]
pub struct AccessControl {
    /// Roles definidos en el sistema
    roles: HashMap<String, Role>,
    /// Asignación de roles a usuarios
    user_roles: HashMap<String, HashSet<String>>,
}

impl AccessControl {
    pub fn new() -> Self {
        AccessControl {
            roles: HashMap::new(),
            user_roles: HashMap::new(),
        }
    }

    /// Definir un nuevo rol
    pub fn define_role(&mut self, name: String, permissions: HashSet<Permission>) {
        self.roles.insert(name.clone(), Role { name, permissions });
    }

    /// Asignar un rol a un usuario
    pub fn assign_role(&mut self, user: &str, role: &str) -> Result<(), String> {
        if !self.roles.contains_key(role) {
            return Err(format!("role '{}' not defined", role));
        }
        self.user_roles
            .entry(user.to_string())
            .or_default()
            .insert(role.to_string());
        Ok(())
    }

    /// Remover un rol de un usuario
    pub fn revoke_role(&mut self, user: &str, role: &str) {
        if let Some(roles) = self.user_roles.get_mut(user) {
            roles.remove(role);
            if roles.is_empty() {
                self.user_roles.remove(user);
            }
        }
    }

    /// Obtener todos los permisos efectivos de un usuario
    pub fn effective_permissions(&self, user: &str) -> HashSet<Permission> {
        let mut perms = HashSet::new();
        if let Some(role_names) = self.user_roles.get(user) {
            for role_name in role_names {
                if let Some(role) = self.roles.get(role_name) {
                    perms.extend(&role.permissions);
                }
            }
        }
        perms
    }

    /// Verificar si un usuario tiene un permiso específico
    pub fn has_permission(&self, user: &str, permission: Permission) -> bool {
        self.effective_permissions(user).contains(&permission)
    }

    /// Obtener los roles de un usuario
    pub fn user_roles(&self, user: &str) -> HashSet<String> {
        self.user_roles.get(user).cloned().unwrap_or_default()
    }

    /// Obtener los permisos de un rol
    pub fn role_permissions(&self, role: &str) -> Option<&HashSet<Permission>> {
        self.roles.get(role).map(|r| &r.permissions)
    }
}
```

### Property tests: tests/access_properties.rs

```rust
use access_control::{AccessControl, Permission};
use proptest::prelude::*;
use std::collections::HashSet;

// ── Estrategias ──────────────────────────────────────────────

fn permission_strategy() -> impl Strategy<Value = Permission> {
    prop_oneof![
        Just(Permission::Read),
        Just(Permission::Write),
        Just(Permission::Delete),
        Just(Permission::Admin),
    ]
}

fn permission_set_strategy() -> impl Strategy<Value = HashSet<Permission>> {
    proptest::collection::hash_set(permission_strategy(), 0..=4)
}

fn username_strategy() -> impl Strategy<Value = String> {
    "[a-z]{3,10}"
}

fn rolename_strategy() -> impl Strategy<Value = String> {
    "[a-z_]{3,12}"
}

#[derive(Debug, Clone)]
enum AcOp {
    DefineRole(String, HashSet<Permission>),
    AssignRole(String, String),  // user, role
    RevokeRole(String, String),  // user, role
    CheckPermission(String, Permission),
}

fn ac_op_strategy() -> impl Strategy<Value = AcOp> {
    prop_oneof![
        (rolename_strategy(), permission_set_strategy())
            .prop_map(|(name, perms)| AcOp::DefineRole(name, perms)),
        (username_strategy(), rolename_strategy())
            .prop_map(|(user, role)| AcOp::AssignRole(user, role)),
        (username_strategy(), rolename_strategy())
            .prop_map(|(user, role)| AcOp::RevokeRole(user, role)),
        (username_strategy(), permission_strategy())
            .prop_map(|(user, perm)| AcOp::CheckPermission(user, perm)),
    ]
}

// ── Propiedad 1: asignar rol otorga sus permisos ─────────────

proptest! {
    #[test]
    fn assign_grants_permissions(
        perms in permission_set_strategy(),
        user in username_strategy(),
        role_name in rolename_strategy()
    ) {
        let mut ac = AccessControl::new();
        ac.define_role(role_name.clone(), perms.clone());
        ac.assign_role(&user, &role_name).unwrap();
        
        let effective = ac.effective_permissions(&user);
        
        // Cada permiso del rol debe estar presente
        for &perm in &perms {
            prop_assert!(
                effective.contains(&perm),
                "usuario '{}' con rol '{}' no tiene permiso {:?}",
                user, role_name, perm
            );
        }
    }
}

// ── Propiedad 2: revocar rol quita sus permisos ──────────────

proptest! {
    #[test]
    fn revoke_removes_permissions(
        perms in permission_set_strategy(),
        user in username_strategy(),
        role_name in rolename_strategy()
    ) {
        let mut ac = AccessControl::new();
        ac.define_role(role_name.clone(), perms.clone());
        ac.assign_role(&user, &role_name).unwrap();
        
        // Verificar que tiene permisos
        let before = ac.effective_permissions(&user);
        prop_assert_eq!(&before, &perms);
        
        // Revocar
        ac.revoke_role(&user, &role_name);
        
        // No debe tener ningún permiso
        let after = ac.effective_permissions(&user);
        prop_assert!(
            after.is_empty(),
            "después de revocar, tiene permisos: {:?}", after
        );
    }
}

// ── Propiedad 3: permisos son la UNIÓN de todos los roles ────

proptest! {
    #[test]
    fn permissions_are_union_of_roles(
        perms1 in permission_set_strategy(),
        perms2 in permission_set_strategy(),
        user in username_strategy(),
        role1 in rolename_strategy(),
    ) {
        // Asegurar que role1 y role2 son diferentes
        let role2 = format!("{}_2", role1);
        
        let mut ac = AccessControl::new();
        ac.define_role(role1.clone(), perms1.clone());
        ac.define_role(role2.clone(), perms2.clone());
        ac.assign_role(&user, &role1).unwrap();
        ac.assign_role(&user, &role2).unwrap();
        
        let effective = ac.effective_permissions(&user);
        let expected: HashSet<_> = perms1.union(&perms2).copied().collect();
        
        prop_assert_eq!(
            effective, expected,
            "permisos efectivos no son unión de roles"
        );
    }
}

// ── Propiedad 4: usuario sin roles no tiene permisos ─────────

proptest! {
    #[test]
    fn no_roles_no_permissions(user in username_strategy()) {
        let ac = AccessControl::new();
        let effective = ac.effective_permissions(&user);
        prop_assert!(effective.is_empty());
    }
}

// ── Propiedad 5: asignar rol no definido falla ───────────────

proptest! {
    #[test]
    fn assign_undefined_role_fails(
        user in username_strategy(),
        role in rolename_strategy()
    ) {
        let mut ac = AccessControl::new();
        // Sin definir el rol
        let result = ac.assign_role(&user, &role);
        prop_assert!(result.is_err());
    }
}

// ── Propiedad 6: asignar un rol no afecta a otros usuarios ──

proptest! {
    #[test]
    fn assign_does_not_affect_others(
        user1 in username_strategy(),
        perms in permission_set_strategy(),
        role in rolename_strategy()
    ) {
        let user2 = format!("{}_other", user1);
        
        let mut ac = AccessControl::new();
        ac.define_role(role.clone(), perms);
        ac.assign_role(&user1, &role).unwrap();
        
        // user2 no debe tener permisos
        let effective = ac.effective_permissions(&user2);
        prop_assert!(
            effective.is_empty(),
            "user2 tiene permisos sin asignarle rol: {:?}", effective
        );
    }
}

// ── Propiedad 7: revocar un rol no afecta otros roles ────────

proptest! {
    #[test]
    fn revoke_does_not_affect_other_roles(
        perms1 in permission_set_strategy().prop_filter("no vacío", |p| !p.is_empty()),
        perms2 in permission_set_strategy().prop_filter("no vacío", |p| !p.is_empty()),
        user in username_strategy(),
        role1 in rolename_strategy()
    ) {
        let role2 = format!("{}_2", role1);
        
        let mut ac = AccessControl::new();
        ac.define_role(role1.clone(), perms1);
        ac.define_role(role2.clone(), perms2.clone());
        ac.assign_role(&user, &role1).unwrap();
        ac.assign_role(&user, &role2).unwrap();
        
        // Revocar solo role1
        ac.revoke_role(&user, &role1);
        
        // role2 sigue activo
        let roles = ac.user_roles(&user);
        prop_assert!(roles.contains(&role2));
        
        // Permisos de role2 siguen presentes
        let effective = ac.effective_permissions(&user);
        for &perm in &perms2 {
            prop_assert!(effective.contains(&perm));
        }
    }
}

// ── Propiedad 8: assign es idempotente ───────────────────────

proptest! {
    #[test]
    fn assign_idempotent(
        perms in permission_set_strategy(),
        user in username_strategy(),
        role in rolename_strategy()
    ) {
        let mut ac1 = AccessControl::new();
        ac1.define_role(role.clone(), perms.clone());
        ac1.assign_role(&user, &role).unwrap();
        let after_once = ac1.effective_permissions(&user);
        
        ac1.assign_role(&user, &role).unwrap();
        let after_twice = ac1.effective_permissions(&user);
        
        prop_assert_eq!(after_once, after_twice);
    }
}

// ── Propiedad 9: revocar es idempotente ──────────────────────

proptest! {
    #[test]
    fn revoke_idempotent(
        perms in permission_set_strategy(),
        user in username_strategy(),
        role in rolename_strategy()
    ) {
        let mut ac = AccessControl::new();
        ac.define_role(role.clone(), perms);
        ac.assign_role(&user, &role).unwrap();
        
        ac.revoke_role(&user, &role);
        let after_once = ac.effective_permissions(&user);
        
        ac.revoke_role(&user, &role); // revocar otra vez
        let after_twice = ac.effective_permissions(&user);
        
        prop_assert_eq!(after_once, after_twice);
    }
}

// ── Propiedad 10: secuencia aleatoria mantiene consistencia ──

proptest! {
    #![proptest_config(ProptestConfig::with_cases(500))]
    
    #[test]
    fn random_ops_consistent(
        ops in proptest::collection::vec(ac_op_strategy(), 1..50)
    ) {
        let mut ac = AccessControl::new();
        
        for op in &ops {
            match op {
                AcOp::DefineRole(name, perms) => {
                    ac.define_role(name.clone(), perms.clone());
                }
                AcOp::AssignRole(user, role) => {
                    let _ = ac.assign_role(user, role); // puede fallar
                }
                AcOp::RevokeRole(user, role) => {
                    ac.revoke_role(user, role);
                }
                AcOp::CheckPermission(user, perm) => {
                    let has = ac.has_permission(user, *perm);
                    let effective = ac.effective_permissions(user);
                    
                    // Consistencia: has_permission = contains en effective_permissions
                    prop_assert_eq!(
                        has,
                        effective.contains(perm),
                        "has_permission inconsistente con effective_permissions"
                    );
                }
            }
        }
        
        // Invariante final: todo usuario con permiso tiene al menos un rol
        // que incluye ese permiso
        // (verificación cruzada de la lógica)
    }
}

// ── Propiedad 11: has_permission coherente ───────────────────

proptest! {
    #[test]
    fn has_permission_coherent(
        perms in permission_set_strategy(),
        user in username_strategy(),
        role in rolename_strategy(),
        query_perm in permission_strategy()
    ) {
        let mut ac = AccessControl::new();
        ac.define_role(role.clone(), perms.clone());
        ac.assign_role(&user, &role).unwrap();
        
        let has = ac.has_permission(&user, query_perm);
        let should_have = perms.contains(&query_perm);
        
        prop_assert_eq!(
            has, should_have,
            "has_permission({:?}) = {}, pero perms del rol: {:?}",
            query_perm, has, perms
        );
    }
}

// ── Propiedad 12: la unión de permisos es conmutativa ────────

proptest! {
    #[test]
    fn role_order_irrelevant(
        perms1 in permission_set_strategy(),
        perms2 in permission_set_strategy(),
        user in username_strategy(),
        role1 in rolename_strategy()
    ) {
        let role2 = format!("{}_b", role1);
        
        // Asignar en orden role1 luego role2
        let mut ac_12 = AccessControl::new();
        ac_12.define_role(role1.clone(), perms1.clone());
        ac_12.define_role(role2.clone(), perms2.clone());
        ac_12.assign_role(&user, &role1).unwrap();
        ac_12.assign_role(&user, &role2).unwrap();
        
        // Asignar en orden role2 luego role1
        let mut ac_21 = AccessControl::new();
        ac_21.define_role(role1.clone(), perms1);
        ac_21.define_role(role2.clone(), perms2);
        ac_21.assign_role(&user, &role2).unwrap();
        ac_21.assign_role(&user, &role1).unwrap();
        
        // Los permisos efectivos deben ser iguales
        prop_assert_eq!(
            ac_12.effective_permissions(&user),
            ac_21.effective_permissions(&user),
            "el orden de asignación de roles afecta los permisos"
        );
    }
}
```

---

## 24. Programa de práctica

### Proyecto: `text_search` — motor de búsqueda de texto con property tests

Implementa un motor de búsqueda de texto simple con índice invertido y verifica propiedades completas.

### src/lib.rs

```rust
use std::collections::{HashMap, HashSet};

/// Documento indexable
#[derive(Debug, Clone)]
pub struct Document {
    pub id: u64,
    pub title: String,
    pub body: String,
}

/// Índice invertido para búsqueda de texto
#[derive(Debug, Clone)]
pub struct SearchIndex {
    /// term → set de document IDs que contienen el term
    index: HashMap<String, HashSet<u64>>,
    /// doc_id → documento
    documents: HashMap<u64, Document>,
}

impl SearchIndex {
    pub fn new() -> Self {
        SearchIndex {
            index: HashMap::new(),
            documents: HashMap::new(),
        }
    }

    /// Tokenizar texto en términos normalizados
    pub fn tokenize(text: &str) -> Vec<String> {
        text.to_lowercase()
            .split(|c: char| !c.is_alphanumeric())
            .filter(|s| !s.is_empty())
            .map(|s| s.to_string())
            .collect()
    }

    /// Indexar un documento
    pub fn add_document(&mut self, doc: Document) {
        let id = doc.id;
        
        // Tokenizar título y cuerpo
        let mut terms = Self::tokenize(&doc.title);
        terms.extend(Self::tokenize(&doc.body));
        
        // Agregar al índice invertido
        for term in terms {
            self.index.entry(term).or_default().insert(id);
        }
        
        self.documents.insert(id, doc);
    }

    /// Buscar documentos que contienen TODOS los términos (AND)
    pub fn search_and(&self, query: &str) -> Vec<&Document> {
        let terms = Self::tokenize(query);
        
        if terms.is_empty() {
            return Vec::new();
        }
        
        let mut result_ids: Option<HashSet<u64>> = None;
        
        for term in &terms {
            let doc_ids = self.index.get(term)
                .cloned()
                .unwrap_or_default();
            
            result_ids = Some(match result_ids {
                Some(existing) => existing.intersection(&doc_ids).copied().collect(),
                None => doc_ids,
            });
        }
        
        let ids = result_ids.unwrap_or_default();
        let mut docs: Vec<&Document> = ids.iter()
            .filter_map(|id| self.documents.get(id))
            .collect();
        docs.sort_by_key(|d| d.id);
        docs
    }

    /// Buscar documentos que contienen AL MENOS UN término (OR)
    pub fn search_or(&self, query: &str) -> Vec<&Document> {
        let terms = Self::tokenize(query);
        
        let mut result_ids = HashSet::new();
        
        for term in &terms {
            if let Some(doc_ids) = self.index.get(term) {
                result_ids.extend(doc_ids);
            }
        }
        
        let mut docs: Vec<&Document> = result_ids.iter()
            .filter_map(|id| self.documents.get(id))
            .collect();
        docs.sort_by_key(|d| d.id);
        docs
    }

    /// Número de documentos indexados
    pub fn document_count(&self) -> usize {
        self.documents.len()
    }

    /// Obtener documento por ID
    pub fn get_document(&self, id: u64) -> Option<&Document> {
        self.documents.get(&id)
    }

    /// Obtener todos los términos del índice
    pub fn terms(&self) -> Vec<&str> {
        self.index.keys().map(|s| s.as_str()).collect()
    }
}
```

### tests/search_properties.rs — Propiedades a implementar

```rust
use text_search::{Document, SearchIndex};
use proptest::prelude::*;

// ── Estrategias ──────────────────────────────────────────────

fn word_strategy() -> impl Strategy<Value = String> {
    "[a-z]{2,8}"
}

fn document_strategy(id: u64) -> impl Strategy<Value = Document> {
    (
        proptest::collection::vec(word_strategy(), 1..5)
            .prop_map(|words| words.join(" ")),
        proptest::collection::vec(word_strategy(), 3..15)
            .prop_map(|words| words.join(" ")),
    )
        .prop_map(move |(title, body)| Document { id, title, body })
}

fn index_with_docs() -> impl Strategy<Value = (SearchIndex, Vec<Document>)> {
    proptest::collection::vec(word_strategy(), 3..15)
        .prop_flat_map(|word_pool| {
            let pool_clone = word_pool.clone();
            (
                Just(word_pool),
                proptest::collection::vec(
                    (
                        proptest::collection::vec(0..pool_clone.len(), 1..4),
                        proptest::collection::vec(0..pool_clone.len(), 2..10),
                    ),
                    1..8,
                )
            )
        })
        .prop_map(|(word_pool, doc_specs)| {
            let mut index = SearchIndex::new();
            let mut docs = Vec::new();
            
            for (id, (title_idxs, body_idxs)) in doc_specs.iter().enumerate() {
                let title = title_idxs.iter()
                    .map(|&i| word_pool[i].clone())
                    .collect::<Vec<_>>()
                    .join(" ");
                let body = body_idxs.iter()
                    .map(|&i| word_pool[i].clone())
                    .collect::<Vec<_>>()
                    .join(" ");
                
                let doc = Document { id: id as u64, title, body };
                index.add_document(doc.clone());
                docs.push(doc);
            }
            
            (index, docs)
        })
}

// ── PROPIEDAD 1: un documento siempre se encuentra por sus propias palabras

proptest! {
    #[test]
    fn document_found_by_own_words((index, docs) in index_with_docs()) {
        for doc in &docs {
            let words = SearchIndex::tokenize(&doc.body);
            for word in &words {
                let results = index.search_or(word);
                let found = results.iter().any(|d| d.id == doc.id);
                prop_assert!(
                    found,
                    "doc {} no encontrado buscando '{}' (body: '{}')",
                    doc.id, word, doc.body
                );
            }
        }
    }
}

// ── PROPIEDAD 2: search_and ⊆ search_or (AND es más restrictivo)

proptest! {
    #[test]
    fn and_subset_of_or(
        (index, _docs) in index_with_docs(),
        query_words in proptest::collection::vec(word_strategy(), 1..4)
    ) {
        let query = query_words.join(" ");
        let and_results: HashSet<u64> = index.search_and(&query)
            .iter().map(|d| d.id).collect();
        let or_results: HashSet<u64> = index.search_or(&query)
            .iter().map(|d| d.id).collect();
        
        // Todo resultado de AND debe estar en OR
        prop_assert!(
            and_results.is_subset(&or_results),
            "AND no es subconjunto de OR"
        );
    }
}

// ── PROPIEDAD 3: search_or con un solo término = search_and

proptest! {
    #[test]
    fn single_term_and_equals_or(
        (index, _docs) in index_with_docs(),
        word in word_strategy()
    ) {
        let and_ids: Vec<u64> = index.search_and(&word)
            .iter().map(|d| d.id).collect();
        let or_ids: Vec<u64> = index.search_or(&word)
            .iter().map(|d| d.id).collect();
        
        prop_assert_eq!(and_ids, or_ids);
    }
}

// ── PROPIEDAD 4: todos los resultados de AND contienen todos los términos

proptest! {
    #[test]
    fn and_results_contain_all_terms(
        (index, _docs) in index_with_docs(),
        query_words in proptest::collection::vec(word_strategy(), 1..3)
    ) {
        let query = query_words.join(" ");
        let results = index.search_and(&query);
        
        for doc in &results {
            let doc_terms: HashSet<String> = SearchIndex::tokenize(&doc.title)
                .into_iter()
                .chain(SearchIndex::tokenize(&doc.body))
                .collect();
            
            for word in &query_words {
                let lower = word.to_lowercase();
                prop_assert!(
                    doc_terms.contains(&lower),
                    "doc {} no contiene '{}' pero apareció en AND",
                    doc.id, word
                );
            }
        }
    }
}

// ── PROPIEDAD 5: todos los resultados de OR contienen al menos un término

proptest! {
    #[test]
    fn or_results_contain_some_term(
        (index, _docs) in index_with_docs(),
        query_words in proptest::collection::vec(word_strategy(), 1..4)
    ) {
        let query = query_words.join(" ");
        let results = index.search_or(&query);
        
        for doc in &results {
            let doc_terms: HashSet<String> = SearchIndex::tokenize(&doc.title)
                .into_iter()
                .chain(SearchIndex::tokenize(&doc.body))
                .collect();
            
            let has_any = query_words.iter()
                .any(|w| doc_terms.contains(&w.to_lowercase()));
            
            prop_assert!(
                has_any,
                "doc {} no contiene ningún término de la query",
                doc.id
            );
        }
    }
}

// ── PROPIEDAD 6: search es determinístico

proptest! {
    #[test]
    fn search_deterministic(
        (index, _docs) in index_with_docs(),
        query in proptest::collection::vec(word_strategy(), 1..3)
            .prop_map(|w| w.join(" "))
    ) {
        let r1: Vec<u64> = index.search_and(&query).iter().map(|d| d.id).collect();
        let r2: Vec<u64> = index.search_and(&query).iter().map(|d| d.id).collect();
        prop_assert_eq!(r1, r2);
    }
}

// ── PROPIEDAD 7: agregar documento incrementa count

proptest! {
    #[test]
    fn add_increments_count(doc in document_strategy(0)) {
        let mut index = SearchIndex::new();
        prop_assert_eq!(index.document_count(), 0);
        
        index.add_document(doc);
        prop_assert_eq!(index.document_count(), 1);
    }
}

// ── PROPIEDAD 8: tokenize es idempotente (normalización)

proptest! {
    #[test]
    fn tokenize_idempotent(s in "[a-zA-Z ]{0,50}") {
        let once = SearchIndex::tokenize(&s);
        for token in &once {
            let re_tokenized = SearchIndex::tokenize(token);
            prop_assert_eq!(
                re_tokenized.len(), 1,
                "re-tokenizar '{}' produjo {:?}", token, re_tokenized
            );
            prop_assert_eq!(
                &re_tokenized[0], token,
                "re-tokenizar cambió '{}' a '{}'", token, re_tokenized[0]
            );
        }
    }
}

// ── PROPIEDAD 9: tokenize produce solo lowercase

proptest! {
    #[test]
    fn tokenize_always_lowercase(s in any::<String>()) {
        let tokens = SearchIndex::tokenize(&s);
        for token in &tokens {
            prop_assert_eq!(
                token, &token.to_lowercase(),
                "token no normalizado: '{}'", token
            );
        }
    }
}

// ── PROPIEDAD 10: query vacía retorna vacío

proptest! {
    #[test]
    fn empty_query_empty_results((index, _docs) in index_with_docs()) {
        prop_assert!(index.search_and("").is_empty());
        prop_assert!(index.search_or("").is_empty());
        prop_assert!(index.search_and("   ").is_empty());
        prop_assert!(index.search_or("   ").is_empty());
    }
}
```

### Ejecutar

```bash
cargo test --test search_properties

# Con más iteraciones
PROPTEST_CASES=2000 cargo test --test search_properties

# Ver detalles de un fallo
cargo test --test search_properties -- --nocapture
```

---

## 25. Ejercicios

### Ejercicio 1: Propiedades para un cache LRU

**Objetivo**: Diseñar y escribir propiedades para una estructura LRU (Least Recently Used) cache.

**Contexto**: un LRU cache con capacidad fija tiene estas operaciones:

```rust
struct LruCache<K, V> {
    fn new(capacity: usize) -> Self;
    fn get(&mut self, key: &K) -> Option<&V>;
    fn put(&mut self, key: K, value: V);
    fn len(&self) -> usize;
    fn capacity(&self) -> usize;
}
```

**Tareas**:

**a)** Enumera al menos 8 propiedades que debe cumplir un LRU cache. Para cada una indica qué tipo de propiedad es (invariante, idempotencia, preservación, etc.).

**b)** Implementa el LRU cache.

**c)** Escribe las 8 propiedades con proptest. Usa testing de máquina de estado con un oracle (`HashMap` sin límite de capacidad) para verificar que `get` retorna los valores correctos.

**d)** Verifica que `len()` nunca excede `capacity()`.

**e)** Implementa un bug intencional (por ejemplo, que evicte el más recientemente usado en vez del menos recientemente usado) y verifica que alguna propiedad lo detecta.

---

### Ejercicio 2: De especificación a propiedades para un rate limiter

**Objetivo**: Traducir una especificación en lenguaje natural a propiedades.

**Especificación del rate limiter**:

```
R1: Cada usuario puede hacer máximo N requests en una ventana de T segundos
R2: Después de agotar el límite, los requests siguientes son rechazados
R3: Después de T segundos sin requests, el contador se reinicia
R4: Usuarios diferentes tienen contadores independientes
R5: El rate limiter nunca bloquea un request si el usuario no ha alcanzado el límite
R6: El rate limiter es determinístico para la misma secuencia de eventos
```

**Tareas**:

**a)** Para cada requisito (R1-R6), escribe al menos una propiedad testeable.

**b)** Implementa el rate limiter con un reloj inyectable (para poder controlar el tiempo en tests).

**c)** Escribe las propiedades con proptest, generando secuencias de `(user, timestamp)`.

**d)** Implementa una propiedad de "monotonía temporal": si un request es aceptado al tiempo T, también sería aceptado al tiempo T+Δ (con Δ suficientemente grande).

---

### Ejercicio 3: Propiedades para un diff de texto

**Objetivo**: Diseñar propiedades para un algoritmo de diff entre dos strings.

**Contexto**: La función `diff(old, new)` retorna una lista de operaciones `[Keep(n), Insert(s), Delete(n)]` que transforman `old` en `new`.

**Tareas**:

**a)** Escribe una función `apply_diff(old, ops) -> String` que aplica las operaciones.

**b)** Diseña y escribe estas propiedades:
- **Round-trip**: `apply_diff(old, diff(old, new)) == new`
- **Identidad**: `diff(s, s)` solo contiene `Keep` (sin inserts ni deletes)
- **Vacío a algo**: `diff("", s)` solo contiene `Insert`
- **Algo a vacío**: `diff(s, "")` solo contiene `Delete`
- **Preservación de longitud**: la suma de las operaciones es coherente con las longitudes de old y new
- **Minimalidad** (propiedad estadística): `diff` no produce operaciones redundantes (no hay `Keep(0)`, `Insert("")`, `Delete(0)`)

**c)** Genera pares `(old, new)` aleatorios y verifica todas las propiedades.

---

### Ejercicio 4: Propiedades compositivas para un evaluador de expresiones

**Objetivo**: Diseñar un conjunto de propiedades que, combinadas, caracterizan completamente un evaluador de expresiones aritméticas.

**Contexto**: un evaluador que soporta +, -, *, / con precedencia y paréntesis.

**Tareas**:

**a)** Diseña un generador de expresiones aritméticas válidas (con `prop_recursive`).

**b)** Escribe estas propiedades individuales (débiles):
- Evaluar un número literal retorna ese número
- Evaluar `a + 0` equivale a evaluar `a`
- Evaluar `a * 1` equivale a evaluar `a`
- Evaluar `a + b` = evaluar `b + a` (conmutatividad)
- Evaluar `a * b` = evaluar `b * a`
- Evaluar `(a + b) + c` = evaluar `a + (b + c)` (asociatividad, con tolerancia float)
- Evaluar `a - a` = 0 para todo `a`

**c)** Argumenta cuáles de estas propiedades son necesarias y cuáles suficientes. Juega al "adversario": ¿puede una función incorrecta pasar todas?

**d)** Agrega una propiedad oracle comparando con una implementación de referencia (puede ser la evaluación directa del AST).

---

## Navegación

- **Anterior**: [T02 - proptest](../T02_Proptest/README.md)
- **Siguiente**: [T04 - proptest vs quickcheck](../T04_Proptest_vs_Quickcheck/README.md)

---

> **Sección 3: Property-Based Testing** — Tópico 3 de 4 completado
>
> - T01: Concepto — generación aleatoria, propiedades invariantes, shrinking ✓
> - T02: proptest — proptest!, any::\<T\>(), estrategias, ProptestConfig, regex ✓
> - T03: Diseñar propiedades — round-trip, invariantes, oracle, idempotencia, algebraicas ✓
> - T04: proptest vs quickcheck (siguiente)
