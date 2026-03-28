# T02 — Flotantes

## f32 y f64

Rust tiene dos tipos de punto flotante: `f32` (32 bits) y `f64`
(64 bits). Ambos siguen el estandar IEEE 754. El tipo por defecto
cuando escribes un literal como `3.14` es `f64`:

```rust
let x = 3.14;           // f64 (tipo inferido por defecto)
let y: f32 = 3.14;      // f32 (anotacion explicita)
let z: f64 = 3.14;      // f64 (anotacion explicita, redundante)
```

```text
// Diferencias entre f32 y f64:
//
// Tipo   Bits   Precision decimal   Rango aproximado
// f32    32     ~7 digitos          +/- 3.4e38
// f64    64     ~15 digitos         +/- 1.8e308
//
// f64 es el tipo por defecto porque en CPUs modernas de 64 bits
// las operaciones con f64 son practicamente igual de rapidas
// que con f32, y la precision extra previene errores sutiles.
//
// Usa f32 cuando:
// - Necesitas ahorrar memoria (millones de valores)
// - Interactuas con APIs que requieren f32 (graficos, audio)
// - La precision de ~7 digitos es suficiente
```

```text
// Layout en memoria segun IEEE 754:
//
// f32 (32 bits):
// [1 bit signo] [8 bits exponente] [23 bits mantisa]
//  S              EEEEEEEE          MMMMMMMMMMMMMMMMMMMMMMM
//
// f64 (64 bits):
// [1 bit signo] [11 bits exponente] [52 bits mantisa]
//  S              EEEEEEEEEEE        MMMM...MMMM (52 bits)
//
// Signo: 0 = positivo, 1 = negativo
// Exponente: codifica la potencia de 2 (con sesgo)
// Mantisa: codifica los digitos significativos
//
// El valor se calcula como: (-1)^S * 2^(E-bias) * 1.M
// donde bias = 127 para f32, 1023 para f64.
```

## Literales

Los literales flotantes se escriben con un punto decimal,
con sufijo de tipo, o con notacion cientifica:

```rust
// Punto decimal obligatorio para que sea flotante:
let a = 3.14;           // f64
let b = 2.0;            // f64 (el .0 lo hace flotante)
let c = 0.5;            // f64

// Sufijo de tipo:
let d = 3.14f32;        // f32
let e = 3.14f64;        // f64
let f = 2.0f32;         // f32
let g = 1f64;           // f64 (el sufijo basta, no necesita punto)

// Notacion cientifica:
let h = 1e10;           // f64: 1 * 10^10 = 10_000_000_000.0
let i = 1.5e-3;         // f64: 1.5 * 10^-3 = 0.0015
let j = 2.5E4;          // f64: 25000.0 (E mayuscula tambien vale)
let k = 1e10f32;        // f32 con notacion cientifica

// Separadores con guion bajo para legibilidad:
let l = 1_000_000.0;    // f64: 1000000.0
let m = 3.141_592_653;  // f64: pi con separadores
let n = 1_234.567_890;  // f64
```

```rust
// Lo que NO es un literal flotante:
// let x = 3;            // esto es i32, no flotante
// let y = .5;           // error: no se puede empezar con punto
// let z = 5.;           // valido: f64, pero poco legible
```

## Valores especiales

IEEE 754 define valores especiales que `f32` y `f64` representan.
Rust los expone como constantes asociadas:

```rust
// Infinitos:
let pos_inf = f64::INFINITY;        // +infinito
let neg_inf = f64::NEG_INFINITY;    // -infinito

// NaN (Not a Number):
let nan = f64::NAN;                 // NaN

// Limites del tipo:
let max = f64::MAX;                 // 1.7976931348623157e308
let min = f64::MIN;                 // -1.7976931348623157e308

// Menor valor positivo (mas cercano a cero sin ser cero):
let min_pos = f64::MIN_POSITIVE;    // 2.2250738585072014e-308

// Epsilon — diferencia entre 1.0 y el siguiente f64 representable:
let eps = f64::EPSILON;             // 2.220446049250313e-16
```

```rust
// Los mismos valores existen para f32:
let max_32 = f32::MAX;             // 3.4028235e38
let eps_32 = f32::EPSILON;         // 1.1920929e-7
// f32::EPSILON es mucho mayor que f64::EPSILON (menos precision).
```

```rust
// Operaciones que producen valores especiales:
let a = 1.0f64 / 0.0;              // f64::INFINITY
let b = -1.0f64 / 0.0;             // f64::NEG_INFINITY
let c = 0.0f64 / 0.0;              // NaN
let d = f64::INFINITY + 1.0;       // f64::INFINITY
let e = f64::INFINITY - f64::INFINITY; // NaN
let f = f64::INFINITY * 0.0;       // NaN
let g = (-1.0f64).sqrt();          // NaN (raiz de negativo)

// En Rust, division por cero con flotantes NO es panic.
// Produce INFINITY o NaN segun el numerador.
// Esto es diferente de enteros, donde division por cero es panic.
```

## NaN

NaN (Not a Number) es un valor especial que representa un resultado
indefinido o no representable. Es el valor mas problematico de los
flotantes porque tiene un comportamiento unico:

```rust
let nan = f64::NAN;

// NaN NO es igual a nada, ni siquiera a si mismo:
println!("{}", nan == nan);         // false
println!("{}", nan == f64::NAN);    // false
println!("{}", nan != nan);         // true

// Esto es por diseno de IEEE 754, no un bug de Rust.
// La razon: NaN representa "resultado indefinido".
// Dos resultados indefinidos no son necesariamente iguales.
```

```rust
// Para detectar NaN, usar el metodo is_nan():
let x = 0.0f64 / 0.0;

// MAL — nunca funciona:
if x == f64::NAN {
    println!("es NaN");    // nunca se ejecuta
}

// BIEN:
if x.is_nan() {
    println!("es NaN");    // se ejecuta
}

// Tambien existe f64::is_nan() como funcion:
println!("{}", f64::is_nan(x));     // true
```

```rust
// NaN es contagioso — se propaga por las operaciones:
let nan = f64::NAN;

let a = nan + 1.0;                 // NaN
let b = nan * 100.0;               // NaN
let c = nan.sqrt();                // NaN
let d = nan.max(1000.0);           // NaN (con el metodo)

// Si un NaN entra en una cadena de calculos,
// TODOS los resultados posteriores seran NaN.
// Es una forma de IEEE 754 de decir "los datos estan corruptos".
```

```rust
// NaN rompe el ordenamiento:
let mut v = vec![3.0, f64::NAN, 1.0, 2.0];

// Esto NO compila:
// v.sort();
// error: the trait `Ord` is not implemented for `f64`

// Esto compila pero da resultados impredecibles:
v.sort_by(|a, b| a.partial_cmp(b).unwrap());
// partial_cmp devuelve None cuando uno de los valores es NaN.
// unwrap() haria panic si hay NaN en el vector.

// Forma segura (Rust 1.62+):
v.sort_by(|a, b| a.total_cmp(b));
// total_cmp define un orden total, incluyendo NaN.
// NaN se ordena despues de INFINITY.
```

## Comparaciones y PartialEq vs Eq

`f64` implementa `PartialEq` pero **no** `Eq`. Implementa
`PartialOrd` pero **no** `Ord`. Esto tiene implicaciones
importantes en como puedes usar flotantes en Rust:

```rust
// PartialEq vs Eq:
//
// Eq requiere que la igualdad sea REFLEXIVA: a == a debe ser true.
// Pero para flotantes, NaN != NaN, lo que viola reflexividad.
// Por eso f64 solo implementa PartialEq (igualdad parcial).
//
// PartialOrd vs Ord:
//
// Ord requiere un ORDEN TOTAL: cualquier par de valores se puede comparar.
// Pero NaN no es menor, mayor, ni igual a nada.
// partial_cmp(NaN, 1.0) devuelve None.
// Por eso f64 solo implementa PartialOrd (orden parcial).
```

```rust
// Consecuencia 1: f64 no puede ser clave de HashMap.
use std::collections::HashMap;

// Esto NO compila:
// let mut map: HashMap<f64, String> = HashMap::new();
// error: the trait `Eq` is not implemented for `f64`
// error: the trait `Hash` is not implemented for `f64`

// HashMap requiere Eq + Hash porque necesita que
// si insertas con clave k, buscar con k siempre la encuentre.
// Con NaN como clave, nunca la encontrarias (NaN != NaN).
```

```rust
// Consecuencia 2: no puedes ordenar Vec<f64> directamente.
let mut v = vec![3.0, 1.0, 2.0];

// NO compila:
// v.sort();
// error: the trait `Ord` is not implemented for `f64`

// Alternativa 1: partial_cmp (panic si hay NaN):
v.sort_by(|a, b| a.partial_cmp(b).unwrap());

// Alternativa 2: total_cmp (seguro, Rust 1.62+):
v.sort_by(|a, b| a.total_cmp(b));

// Alternativa 3: sort_unstable_by con total_cmp (mas rapido):
v.sort_unstable_by(|a, b| a.total_cmp(b));
```

```rust
// Consecuencia 3: no puedes usar f64 en match con rangos exhaustivos,
// ni como discriminante de enum, ni en ningun contexto que requiera Eq/Ord.

// El metodo total_cmp() (Rust 1.62+):
// Define un orden total compatible con IEEE 754 totalOrder:
// -NaN < -INF < ... < -0.0 < +0.0 < ... < +INF < +NaN
//
// Devuelve std::cmp::Ordering (Less, Equal, Greater), nunca falla.
let a = 1.0f64;
let b = 2.0f64;
println!("{:?}", a.total_cmp(&b));   // Less
```

```rust
// Solucion robusta: el crate ordered-float.
// Proporciona wrappers que implementan Eq, Ord y Hash:
//
// # En Cargo.toml:
// [dependencies]
// ordered-float = "4"
//
// use ordered_float::OrderedFloat;
// let mut map: HashMap<OrderedFloat<f64>, String> = HashMap::new();
// map.insert(OrderedFloat(1.0), "one".to_string());
//
// let mut v = vec![OrderedFloat(3.0), OrderedFloat(1.0), OrderedFloat(2.0)];
// v.sort();  // funciona — OrderedFloat implementa Ord
```

## Precision

Los numeros de punto flotante no pueden representar todos los
decimales con exactitud. Esto es una propiedad fundamental de
la representacion binaria, no un bug de Rust:

```rust
// El clasico problema de precision:
let x = 0.1 + 0.2;
println!("{}", x);              // 0.30000000000000004
println!("{}", x == 0.3);      // false

// Razon: 0.1 en binario es una fraccion periodica infinita,
// como 1/3 en decimal (0.333...).
// Se almacena una aproximacion, y las aproximaciones se acumulan.
```

```rust
// Mas ejemplos de perdida de precision:
println!("{:.20}", 0.1f64);     // 0.10000000000000000555
println!("{:.20}", 0.2f64);     // 0.20000000000000001110
println!("{:.20}", 0.3f64);     // 0.29999999999999998890

// La suma 0.1 + 0.2 da 0.30000000000000004
// que NO es igual al 0.3 almacenado (0.29999999999999998890).
```

```rust
// Comparacion con epsilon absoluto:
fn approx_eq(a: f64, b: f64, epsilon: f64) -> bool {
    (a - b).abs() < epsilon
}

let x = 0.1 + 0.2;
println!("{}", approx_eq(x, 0.3, f64::EPSILON));   // true para valores ~1.0
println!("{}", approx_eq(x, 0.3, 1e-10));           // true

// f64::EPSILON (~2.2e-16) es la diferencia entre 1.0 y el siguiente
// f64 representable. Funciona bien para comparar valores cercanos a 1.0
// pero NO escala para valores muy grandes o muy pequenos.
```

```rust
// Comparacion con epsilon relativo (mas robusta para cualquier magnitud):
fn relative_eq(a: f64, b: f64, epsilon: f64) -> bool {
    if a == b { return true; }      // maneja infinitos y cero
    let diff = (a - b).abs();
    let largest = a.abs().max(b.abs());
    diff <= largest * epsilon
}
// Con epsilon absoluto, comparar 1_000_000.1 y 1_000_000.2 fallaria.
// Con epsilon relativo funciona para cualquier magnitud.
```

```rust
// Perdida de precision por magnitud (absorcion):
let big = 1e16_f64;
println!("{}", big + 1.0 == big);       // true
// 1.0 es tan pequeno comparado con 1e16 que se "absorbe".
// f64 solo tiene ~15 digitos significativos.
// Solucion: algoritmo de suma de Kahan, o sumar de menor a mayor.
```

## Conversion

La conversion entre tipos flotantes y entre flotantes y enteros
requiere atencion porque puede haber perdida de precision o de rango:

```rust
// f32 a f64 — ampliacion (siempre segura):
let x: f32 = 3.14;
let y: f64 = x as f64;             // 3.140000104904175 (precision f32 se mantiene)
let z: f64 = f64::from(x);         // igual, pero mas explicito
// No pierde informacion: f64 puede representar todo lo que f32 puede.
```

```rust
// f64 a f32 — reduccion (potencialmente lossy):
let x: f64 = 3.141592653589793;
let y: f32 = x as f32;             // 3.1415927 (pierde digitos)

// Valores fuera de rango de f32:
let big: f64 = 1e40;
let narrowed: f32 = big as f32;    // f32::INFINITY (desborda)

let small: f64 = 1e-50;
let narrowed: f32 = small as f32;  // 0.0 (subdesborda)
```

```rust
// Flotante a entero — truncamiento (desde Rust 1.45, saturacion):
let x = 3.99f64;
let n = x as i32;                  // 3 (trunca, no redondea)

let big = 1e18f64;
let n = big as i32;                // i32::MAX (2147483647) — satura

let neg_big = -1e18f64;
let n = neg_big as i32;            // i32::MIN (-2147483648) — satura

let nan = f64::NAN;
let n = nan as i32;                // 0 — NaN se convierte en 0

let inf = f64::INFINITY;
let n = inf as i32;                // i32::MAX — satura

// Antes de Rust 1.45 estos casos eran comportamiento indefinido.
// Ahora la conversion as siempre satura: el valor se ajusta
// al limite mas cercano del tipo destino.
```

```rust
// Entero a flotante:
let n: i32 = 42;
let x = n as f64;                  // 42.0 (exacto para i32 a f64)

let big: i64 = 9_007_199_254_740_993;  // 2^53 + 1
let x = big as f64;               // 9007199254740992.0 (pierde precision)
// f64 tiene 52 bits de mantisa, asi que enteros mayores a 2^53
// no se representan exactamente.

let n: i32 = 42;
let x: f32 = n as f32;            // 42.0
// f32 puede representar exactamente enteros hasta 2^24 (16_777_216).
```

```rust
// Conversiones seguras con From/Into:
// Solo existen las conversiones que son lossless:
let x: f64 = f64::from(3.14f32);   // f32 -> f64 (ok, ampliacion)
let y: f64 = f64::from(42i32);     // i32 -> f64 (ok, exacto)
let z: f64 = f64::from(42i16);     // i16 -> f64 (ok, exacto)

// Estas NO existen (serian lossy):
// let a: f32 = f32::from(3.14f64);   // error: no impl
// let b: i32 = i32::from(3.14f64);   // error: no impl
// let c: f64 = f64::from(42i64);     // error: i64 puede perder precision

// Regla: From/Into solo implementa conversiones que nunca pierden datos.
// Para conversiones lossy, usa "as" y acepta la responsabilidad.
```

## Metodos utiles

`f64` (y `f32`) tienen muchos metodos integrados en la libreria
estandar. No necesitas crates externos para matematica basica:

```rust
// Valor absoluto:
let x = -3.14f64;
println!("{}", x.abs());            // 3.14

// Redondeo:
let x = 3.7f64;
println!("{}", x.floor());         // 3.0 (redondea hacia abajo)
println!("{}", x.ceil());          // 4.0 (redondea hacia arriba)
println!("{}", x.round());         // 4.0 (redondea al mas cercano)
println!("{}", x.trunc());         // 3.0 (trunca la parte decimal)
println!("{}", x.fract());         // 0.7 (parte fraccionaria)

let y = -3.7f64;
println!("{}", y.floor());         // -4.0
println!("{}", y.ceil());          // -3.0
println!("{}", y.round());         // -4.0
println!("{}", y.trunc());         // -3.0
```

```rust
// Raiz cuadrada y potencias:
let x = 9.0f64;
println!("{}", x.sqrt());          // 3.0

let base = 2.0f64;
println!("{}", base.powi(10));     // 1024.0 (potencia entera, rapida)
println!("{}", base.powf(0.5));    // 1.4142... (potencia flotante)

// powi(n) es mas rapido que powf(n as f64) porque usa
// exponenciacion por cuadrados con aritmetica entera.
```

```rust
// Trigonometria (angulos en radianes):
use std::f64::consts::PI;

println!("{}", (PI / 2.0).sin());   // 1.0
println!("{}", PI.cos());           // -1.0
println!("{}", (PI / 4.0).tan());   // 1.0 (aprox)
println!("{}", (1.0f64).asin());    // 1.5707... (PI/2)
println!("{}", (1.0f64).atan2(1.0));// 0.7853... (PI/4)

// Convertir grados a radianes:
let degrees = 90.0f64;
let radians = degrees.to_radians(); // PI/2
let back = radians.to_degrees();    // 90.0
```

```rust
// Minimo, maximo y clamping:
let a = 3.0f64;
let b = 7.0f64;

println!("{}", a.min(b));           // 3.0
println!("{}", a.max(b));           // 7.0

// clamp limita un valor a un rango:
let x = 15.0f64;
println!("{}", x.clamp(0.0, 10.0)); // 10.0

let y = -5.0f64;
println!("{}", y.clamp(0.0, 10.0)); // 0.0

let z = 5.0f64;
println!("{}", z.clamp(0.0, 10.0)); // 5.0

// Nota: si un argumento es NaN, min/max devuelven el otro valor.
```

```rust
// Verificaciones de estado:
let values = [1.0f64, f64::INFINITY, f64::NEG_INFINITY, f64::NAN, 0.0];

for v in &values {
    println!(
        "{:>6} -> finite={}, infinite={}, nan={}, sign_pos={}",
        v,
        v.is_finite(),          // no es ni infinito ni NaN
        v.is_infinite(),        // es +INF o -INF
        v.is_nan(),             // es NaN
        v.is_sign_positive(),   // signo positivo (incluso +0.0)
    );
}
// Salida:
//      1 -> finite=true,  infinite=false, nan=false, sign_pos=true
//    inf -> finite=false, infinite=true,  nan=false, sign_pos=true
//   -inf -> finite=false, infinite=true,  nan=false, sign_pos=false
//    NaN -> finite=false, infinite=false, nan=true,  sign_pos=true
//      0 -> finite=true,  infinite=false, nan=false, sign_pos=true
```

```rust
// Otros metodos utiles:
let x = -0.0f64;
println!("{}", x.is_sign_negative());   // true (-0.0 tiene signo negativo)

let mag = 5.0f64;
println!("{}", mag.copysign(-1.0));     // -5.0 (copia el signo)

// Hipotenusa (evita overflow intermedio):
let a = 3.0f64;
let b = 4.0f64;
println!("{}", a.hypot(b));             // 5.0

// Multiply-add fusionado (mas preciso que a*b + c):
println!("{}", 2.0f64.mul_add(3.0, 4.0)); // 10.0

// Logaritmos y exponencial:
println!("{}", 10.0f64.ln());          // 2.302... (log natural)
println!("{}", 10.0f64.log2());        // 3.321... (log base 2)
println!("{}", 10.0f64.log10());       // 1.0 (log base 10)
println!("{}", 1.0f64.exp());          // 2.718... (e^1)
```

---

## Ejercicios

### Ejercicio 1 — Explorar precision

```rust
// Escribir un programa que:
// 1. Calcule 0.1 + 0.2 y lo compare con 0.3 usando ==
// 2. Imprima ambos valores con 20 decimales (formato {:.20})
// 3. Implemente una funcion approx_eq(a, b, epsilon) que compare
//    con tolerancia y la use para verificar 0.1 + 0.2 ~= 0.3
// 4. Repita con f32 y observe la diferencia de precision
//
// Preguntas:
// - Cual es la diferencia entre los valores impresos?
// - Funciona f64::EPSILON como epsilon? Y 1e-10?
// - Es la precision de f32 peor o mejor de lo esperado?
```

### Ejercicio 2 — Valores especiales y NaN

```rust
// Escribir un programa que:
// 1. Genere NaN de tres formas distintas (0.0/0.0, sqrt(-1), etc.)
// 2. Verifique que NaN != NaN usando == y luego con is_nan()
// 3. Demuestre la propagacion: cree una cadena de operaciones
//    donde un NaN inicial corrompa 5 calculos sucesivos
// 4. Cree un Vec<f64> con algunos NaN y ordene con total_cmp()
// 5. Imprima el vector ordenado y observe donde quedan los NaN
//
// Preguntas:
// - Que pasa si usas partial_cmp().unwrap() con NaN en el vector?
// - Donde coloca total_cmp a NaN en el orden?
```

### Ejercicio 3 — Conversiones y limites

```rust
// Escribir un programa que:
// 1. Convierta f32 -> f64 y f64 -> f32, imprima con 20 decimales
//    y observe la perdida de precision en la reduccion
// 2. Convierta f64::MAX a f32 y observe el resultado
// 3. Convierta f64::NAN a i32, f64::INFINITY a i32,
//    y un valor normal como 3.99 a i32. Documente cada resultado.
// 4. Encuentre el entero mas grande que f64 puede representar
//    exactamente (pista: 2^53) y demuestre que 2^53 + 1 pierde precision
// 5. Use From/Into para las conversiones seguras y as para las lossy.
//    Intente From donde no existe y observe el error del compilador.
//
// Preguntas:
// - Por que f64::from(i64) no existe pero f64::from(i32) si?
// - Que valor produce NaN as i32? Es intuitivo?
```
