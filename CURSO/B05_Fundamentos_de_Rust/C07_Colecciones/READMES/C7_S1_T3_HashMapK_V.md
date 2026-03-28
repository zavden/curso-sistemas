# T03 — HashMap\<K, V\>

## Que es HashMap

`HashMap<K, V>` es una tabla hash: almacena pares clave-valor
con busqueda, insercion y eliminacion en O(1) promedio.

```rust
use std::collections::HashMap;
// HashMap NO esta en el preludio — hay que importarlo siempre

let mut scores: HashMap<String, i32> = HashMap::new();
scores.insert(String::from("Alice"), 95);
scores.insert(String::from("Bob"), 87);
scores.insert(String::from("Carol"), 92);

println!("{:?}", scores);
// {"Alice": 95, "Bob": 87, "Carol": 92}
// (el orden NO esta garantizado — puede variar entre ejecuciones)
```

```text
HashMap internamente:
- Usa una tabla hash con Robin Hood hashing (SwissTable desde Rust 1.36)
- El hasher por defecto es SipHash 1-3 (resistente a HashDoS)
- Los pares clave-valor se almacenan en el heap
- El orden de iteracion es NO determinista
```

## Crear un HashMap

```rust
use std::collections::HashMap;

// 1. HashMap::new()
let mut map: HashMap<String, i32> = HashMap::new();

// 2. HashMap::with_capacity() — pre-alocar
let mut map: HashMap<String, i32> = HashMap::with_capacity(100);
// Evita realocaciones si sabes cuantos elementos tendras

// 3. collect() desde iterador de tuplas
let teams = vec![("Red", 3), ("Blue", 5), ("Green", 2)];
let scores: HashMap<&str, i32> = teams.into_iter().collect();

// 4. collect() desde zip de dos iteradores
let keys = vec!["Alice", "Bob", "Carol"];
let values = vec![95, 87, 92];
let scores: HashMap<&str, i32> = keys.into_iter().zip(values).collect();

// 5. HashMap::from() — desde array de tuplas (Rust 1.56+)
let scores = HashMap::from([
    ("Alice", 95),
    ("Bob", 87),
    ("Carol", 92),
]);
```

## Insercion

```rust
use std::collections::HashMap;

let mut map = HashMap::new();

// insert — agrega o sobreescribe
map.insert("key1", 10);
map.insert("key2", 20);

// Si la clave ya existe, insert SOBREESCRIBE y retorna el valor anterior:
let old = map.insert("key1", 99);
assert_eq!(old, Some(10));  // retorna el valor previo

let old = map.insert("key3", 30);
assert_eq!(old, None);  // no habia valor previo
```

```rust
// Ownership en insert:
let mut map = HashMap::new();

let key = String::from("name");
let val = String::from("Alice");

map.insert(key, val);
// key y val fueron MOVIDOS al HashMap
// println!("{key}"); // error: key fue movido
// println!("{val}"); // error: val fue movido

// Tipos Copy (i32, &str, etc.) se copian, no se mueven:
let mut map = HashMap::new();
let key = "name";  // &str es Copy
let val = 42;      // i32 es Copy
map.insert(key, val);
println!("{key}: {val}");  // OK — se copiaron
```

## Acceso

```rust
use std::collections::HashMap;

let mut map = HashMap::from([
    ("Alice", 95),
    ("Bob", 87),
]);

// get — retorna Option<&V>
let score = map.get("Alice");    // Some(&95)
let score = map.get("Unknown");  // None

// [] — panic si la clave no existe
let score = map["Alice"];        // 95
// let bad = map["Unknown"];     // panic: key not found

// get_mut — referencia mutable al valor
if let Some(score) = map.get_mut("Alice") {
    *score += 5;  // Alice ahora tiene 100
}

// contains_key
assert!(map.contains_key("Alice"));
assert!(!map.contains_key("Unknown"));

// len / is_empty
assert_eq!(map.len(), 2);
assert!(!map.is_empty());
```

```text
Regla practica:
  map.get(k)     → cuando la clave puede no existir (Option)
  map[k]         → cuando SABES que existe (panic si no)
  map.get_mut(k) → cuando necesitas modificar el valor
```

## Eliminacion

```rust
use std::collections::HashMap;

let mut map = HashMap::from([
    ("Alice", 95),
    ("Bob", 87),
    ("Carol", 92),
]);

// remove — quita y retorna el valor
let removed = map.remove("Bob");
assert_eq!(removed, Some(87));
assert_eq!(map.remove("Bob"), None);  // ya no existe

// remove_entry — quita y retorna clave + valor
let mut map = HashMap::from([("key".to_string(), 42)]);
let (k, v) = map.remove_entry("key").unwrap();
// k = String("key"), v = 42 — recuperas ownership de ambos

// clear — vacia todo
map.clear();
assert!(map.is_empty());

// retain — filtra in-place
let mut map = HashMap::from([("a", 1), ("b", 2), ("c", 3), ("d", 4)]);
map.retain(|_key, val| *val % 2 == 0);
// map = {"b": 2, "d": 4}
```

## La Entry API

La entry API es la forma idiomatica de insertar condicionalmente
o modificar valores existentes. Evita hacer dos lookups (uno para
verificar y otro para insertar):

```rust
use std::collections::HashMap;

let mut scores: HashMap<String, i32> = HashMap::new();

// or_insert — inserta si la clave NO existe, retorna &mut V
scores.entry("Alice".to_string()).or_insert(95);
scores.entry("Alice".to_string()).or_insert(0);  // no sobreescribe
assert_eq!(scores["Alice"], 95);  // sigue en 95

// or_insert_with — inserta con closure (lazy, solo si falta)
scores.entry("Bob".to_string()).or_insert_with(|| {
    println!("calculando valor...");
    expensive_computation()
});

// or_default — inserta el valor Default del tipo
let mut counts: HashMap<String, i32> = HashMap::new();
counts.entry("hello".to_string()).or_default();
// i32::default() == 0
```

```rust
// Caso de uso principal: conteo de frecuencias
let text = "hello world hello rust hello world";
let mut counts: HashMap<&str, i32> = HashMap::new();

for word in text.split_whitespace() {
    let count = counts.entry(word).or_insert(0);
    *count += 1;
}
// {"hello": 3, "world": 2, "rust": 1}
```

```rust
// and_modify — modificar si existe, or_insert si no
let mut map: HashMap<&str, Vec<i32>> = HashMap::new();

map.entry("nums")
    .and_modify(|v| v.push(2))
    .or_insert_with(|| vec![1]);
// "nums" no existia → vec![1]

map.entry("nums")
    .and_modify(|v| v.push(2))
    .or_insert_with(|| vec![1]);
// "nums" existe → push(2) → vec![1, 2]
```

```rust
// Patron: agrupar elementos
let data = vec![("fruit", "apple"), ("vegetable", "carrot"),
                ("fruit", "banana"), ("vegetable", "broccoli")];

let mut groups: HashMap<&str, Vec<&str>> = HashMap::new();

for (category, item) in &data {
    groups.entry(category).or_default().push(item);
}
// {"fruit": ["apple", "banana"], "vegetable": ["carrot", "broccoli"]}
```

```text
Flujo de entry():

map.entry(key)
  → Entry::Occupied(entry)     ← la clave existe
  → Entry::Vacant(entry)       ← la clave no existe

  .or_insert(val)              → inserta val si Vacant
  .or_insert_with(|| val)      → inserta con closure si Vacant
  .or_default()                → inserta Default::default() si Vacant
  .and_modify(|v| ...)         → modifica si Occupied (encadenable)

Todos retornan &mut V — puedes modificar el valor despues.
```

## Iteracion

```rust
use std::collections::HashMap;

let map = HashMap::from([
    ("Alice", 95),
    ("Bob", 87),
    ("Carol", 92),
]);

// Iterar sobre pares (&K, &V)
for (name, score) in &map {
    println!("{name}: {score}");
}

// Iterar sobre pares (&K, &mut V)
let mut map = map;
for (name, score) in &mut map {
    *score += 10;  // bonus para todos
}

// Solo claves
for name in map.keys() {
    println!("{name}");
}

// Solo valores
for score in map.values() {
    println!("{score}");
}

// Valores mutables
for score in map.values_mut() {
    *score *= 2;
}

// Consumir el HashMap — mueve claves y valores
for (name, score) in map {
    // name: &str (Copy), score: i32 (Copy)
    println!("{name} scored {score}");
}
// map ya no existe
```

```text
IMPORTANTE: el orden de iteracion de HashMap es NO determinista.
No dependas del orden. Si necesitas orden:
  - Ordenado por clave → BTreeMap
  - Orden de insercion → indexmap (crate externo)
```

## Por que K necesita Hash + Eq

Para funcionar como clave en un HashMap, un tipo debe implementar
dos traits:

```text
Hash — permite calcular un hash numerico del valor.
       El hash se usa para encontrar el "bucket" en la tabla.

Eq   — permite comparar dos claves por igualdad exacta.
       Cuando dos claves tienen el mismo hash (colision),
       se usa Eq para distinguirlas.

Regla critica:
  Si a == b (Eq), entonces hash(a) == hash(b)
  (pero no al reves — dos valores distintos pueden tener el mismo hash)
```

```rust
// Tipos que implementan Hash + Eq (pueden ser claves):
let mut map: HashMap<i32, &str> = HashMap::new();     // OK
let mut map: HashMap<String, i32> = HashMap::new();   // OK
let mut map: HashMap<(i32, i32), &str> = HashMap::new(); // OK
let mut map: HashMap<bool, i32> = HashMap::new();     // OK

// Tipos que NO implementan Hash o Eq:
// let mut map: HashMap<f64, &str> = HashMap::new();
// error: f64 no implementa Eq (por NaN: NaN != NaN)

// let mut map: HashMap<Vec<i32>, &str> = HashMap::new();
// OK — Vec implementa Hash y Eq (si T los implementa)

// Structs custom — necesitan derive o implementacion manual:
#[derive(Hash, Eq, PartialEq)]
struct Point {
    x: i32,
    y: i32,
}
let mut map: HashMap<Point, &str> = HashMap::new();
map.insert(Point { x: 0, y: 0 }, "origin");
```

```rust
// ¿Por que f64 no puede ser clave?
// Porque NaN != NaN (viola la propiedad reflexiva de Eq)
let x = f64::NAN;
assert!(x != x);  // NaN no es igual a si mismo

// Si permitieras f64 como clave:
//   map.insert(NaN, "value");
//   map.get(&NaN)  → ¿deberia encontrarlo? NaN != NaN → no lo encuentra
//   → el valor queda "atrapado" en el map, imposible de recuperar

// Solucion si necesitas floats como clave:
// Usar un wrapper que implemente Eq (ej: ordered_float::OrderedFloat)
// o convertir a una representacion entera (bits).
```

## Hashing y el hasher

```rust
// HashMap usa SipHash 1-3 por defecto — resistente a HashDoS
// HashDoS: un atacante envia claves que colisionan intencionalmente,
// degradando O(1) a O(n) y causando denial-of-service.

// SipHash es mas lento que hashes simples (FxHash, AHash) pero seguro.
// Para casos donde no hay input externo, puedes usar hashers mas rapidos:

use std::collections::HashMap;
use std::hash::BuildHasherDefault;

// Con hasher custom (requiere crate, ej: rustc_hash o ahash):
// use rustc_hash::FxHashMap;
// let mut map: FxHashMap<i32, String> = FxHashMap::default();
// ~2x mas rapido que SipHash para claves pequeñas (i32, u64)

// O con ahash (hasher rapido y razonablemente seguro):
// use ahash::AHashMap;
// let mut map: AHashMap<String, i32> = AHashMap::new();
```

```text
¿Cuando cambiar el hasher?
- Perfiling muestra que hashing es el cuello de botella
- Las claves son pequeñas (numeros, enums) y no vienen de input externo
- Compiladores, interpretadores, herramientas internas

Para la mayoria de aplicaciones, el SipHash por defecto esta bien.
```

## Implementar Hash y Eq para tipos custom

```rust
use std::collections::HashMap;
use std::hash::{Hash, Hasher};

// Opcion 1: derive (lo mas comun)
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
struct Color {
    r: u8,
    g: u8,
    b: u8,
}

let mut map = HashMap::new();
map.insert(Color { r: 255, g: 0, b: 0 }, "red");
map.insert(Color { r: 0, g: 0, b: 255 }, "blue");

// Opcion 2: implementacion manual (cuando no todos los campos importan)
#[derive(Debug)]
struct Employee {
    id: u64,
    name: String,
    department: String,
}

// Solo usar 'id' para igualdad y hash:
impl PartialEq for Employee {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl Eq for Employee {}

impl Hash for Employee {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
        // IMPORTANTE: si PartialEq solo usa id,
        // Hash tambien debe usar SOLO id.
        // Si a == b, entonces hash(a) == hash(b)
    }
}
```

```text
Regla de consistencia Hash/Eq:

  Si implementas Eq manualmente (comparando solo algunos campos),
  Hash DEBE usar exactamente los mismos campos.

  MAL:  Eq compara solo id, pero Hash usa id + name
        → dos employees con mismo id pero distinto name
          tendrian Eq true pero hash diferente → UB en HashMap

  MAL:  Eq compara id + name, pero Hash solo usa id
        → no es UB, pero genera colisiones innecesarias

  BIEN: Eq y Hash usan exactamente los mismos campos
```

## Patrones comunes

```rust
use std::collections::HashMap;

// 1. Conteo de frecuencias
fn char_frequency(s: &str) -> HashMap<char, usize> {
    let mut freq = HashMap::new();
    for c in s.chars() {
        *freq.entry(c).or_insert(0) += 1;
    }
    freq
}

// 2. Invertir un map
fn invert(map: &HashMap<String, String>) -> HashMap<&str, &str> {
    map.iter().map(|(k, v)| (v.as_str(), k.as_str())).collect()
}

// 3. Merge de dos maps
fn merge(a: HashMap<String, i32>, b: HashMap<String, i32>) -> HashMap<String, i32> {
    let mut result = a;
    for (k, v) in b {
        *result.entry(k).or_insert(0) += v;
    }
    result
}

// 4. Cache / memoization
fn fibonacci_memo(n: u64, cache: &mut HashMap<u64, u64>) -> u64 {
    if n <= 1 {
        return n;
    }
    if let Some(&val) = cache.get(&n) {
        return val;
    }
    let result = fibonacci_memo(n - 1, cache) + fibonacci_memo(n - 2, cache);
    cache.insert(n, result);
    result
}

// 5. Lookup con valor por defecto
let config = HashMap::from([("port", "8080"), ("host", "localhost")]);
let timeout = config.get("timeout").copied().unwrap_or("30");
```

## Errores comunes

```rust
use std::collections::HashMap;

// ERROR 1: asumir orden de iteracion
let map = HashMap::from([("a", 1), ("b", 2), ("c", 3)]);
let keys: Vec<&str> = map.keys().collect();
// keys puede ser ["b", "a", "c"] o cualquier permutacion
// Si necesitas orden: .keys().sorted() o usa BTreeMap

// ERROR 2: borrow checker con get + insert
let mut map = HashMap::from([("key", vec![1, 2])]);
// let v = map.get("key").unwrap();
// map.insert("other", vec![3]);  // error: ya hay un borrow inmutable
// Solucion: usar entry, o clonar, o reestructurar

// ERROR 3: usar & como clave cuando insertas String
let mut map: HashMap<String, i32> = HashMap::new();
map.insert("key".to_string(), 42);
// map.get("key")  → funciona gracias a Borrow trait
// No necesitas: map.get(&"key".to_string())

// ERROR 4: Hash inconsistente con Eq
// (ver seccion de Hash/Eq arriba)

// ERROR 5: olvidar importar HashMap
// use std::collections::HashMap;  // necesario — no esta en el preludio
```

## Cheatsheet

```text
Crear:
  HashMap::new()                 vacio
  HashMap::with_capacity(n)      pre-alocar
  HashMap::from([(k,v), ...])    desde array
  iter.collect()                 desde iterador de (K,V)

Insertar/modificar:
  map.insert(k, v)               sobreescribe si existe
  map.entry(k).or_insert(v)      solo si no existe
  map.entry(k).or_default()      inserta Default si no existe
  map.entry(k).and_modify(|v|..) modifica si existe

Acceso:
  map.get(&k)                    Option<&V>
  map[&k]                        &V (panic si no existe)
  map.get_mut(&k)                Option<&mut V>
  map.contains_key(&k)           bool

Eliminar:
  map.remove(&k)                 Option<V>
  map.retain(|k,v| cond)         filtrar in-place
  map.clear()                    vaciar

Iterar:
  for (k, v) in &map             pares (&K, &V)
  for (k, v) in &mut map         pares (&K, &mut V)
  map.keys()                     solo claves
  map.values() / values_mut()    solo valores

Info:
  map.len()                      numero de pares
  map.is_empty()                 len == 0
  map.capacity()                 espacio alocado

Requisitos para K:
  Hash + Eq (derivar o implementar manualmente)
```

---

## Ejercicios

### Ejercicio 1 — Conteo de palabras con entry

```rust
// Escribe fn word_count(text: &str) -> HashMap<String, usize>
// que cuente la frecuencia de cada palabra (case-insensitive).
//
// Usa la entry API (no get + insert por separado).
//
// Para "The cat and the dog and the bird":
// Predice el resultado antes de ejecutar.
// ¿Cuantas entradas tiene el map?
```

### Ejercicio 2 — Agrupacion

```rust
// Dado un Vec<(String, String)> de (estudiante, materia),
// construye un HashMap<String, Vec<String>> donde:
//   clave = materia
//   valor = lista de estudiantes inscritos
//
// Datos:
//   [("Alice", "Math"), ("Bob", "Science"), ("Alice", "Science"),
//    ("Carol", "Math"), ("Bob", "Math")]
//
// Predice: ¿que contiene la clave "Math"? ¿Y "Science"?
// Usa entry().or_default().push()
```

### Ejercicio 3 — Struct como clave

```rust
// Define un struct Point { x: i32, y: i32 }
// Derivale los traits necesarios para usarlo como clave de HashMap.
//
// Crea un HashMap<Point, String> que mapee coordenadas a nombres:
//   (0, 0) → "origin"
//   (1, 0) → "east"
//   (0, 1) → "north"
//
// Verifica que:
//   map.get(&Point{x:0, y:0}) == Some(&"origin".to_string())
//   map.contains_key(&Point{x:5, y:5}) == false
//
// Bonus: ¿que pasa si olvidas derivar Hash? ¿Y si olvidas Eq?
// Predice el error del compilador antes de probarlo.
```
