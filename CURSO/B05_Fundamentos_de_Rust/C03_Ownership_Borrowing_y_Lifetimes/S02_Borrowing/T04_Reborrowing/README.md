# T04 — Reborrowing

## El problema

`&mut T` no implementa `Copy`. Y tiene sentido: si pudieras copiar
una referencia mutable, tendrias dos `&mut T` al mismo dato al mismo
tiempo. Eso viola la regla fundamental de exclusividad de Rust.

```rust
// &mut T NO es Copy.
// Este codigo NO compila:
let mut x = 42;
let r = &mut x;
let r2 = r;     // r se MUEVE a r2
println!("{}", r); // ERROR: r ya no es valido (fue movido)
```

Pero entonces, este codigo deberia fallar:

```rust
fn bar(v: &mut Vec<i32>) {
    v.push(100);
}

fn baz(v: &mut Vec<i32>) {
    v.push(200);
}

fn foo(v: &mut Vec<i32>) {
    bar(v);  // si v se mueve aqui...
    baz(v);  // ...esto deberia ser un error. Pero compila.
}

// Segun las reglas de move, pasar v a bar() deberia consumirlo.
// Despues de bar(v), la variable v ya no deberia ser valida.
// Sin embargo, el codigo compila y funciona correctamente.
//
// La razon: reborrowing.
```

## Reborrowing explicado

Cuando pasas `v` (de tipo `&mut T`) a una funcion que espera `&mut T`,
el compilador **no** mueve `v`. En su lugar, crea un **reborrow**
temporal: una nueva referencia mutable que toma prestado de `v`.

```rust
fn bar(v: &mut Vec<i32>) {
    v.push(100);
}

fn foo(v: &mut Vec<i32>) {
    // Lo que tu escribes:
    bar(v);

    // Lo que el compilador hace internamente:
    bar(&mut *v);
    //  ^^^^^^^ reborrow: crea un nuevo &mut que toma prestado de v

    // Paso a paso:
    // 1. *v  — desreferencia v para llegar al Vec<i32>
    // 2. &mut *v — crea una NUEVA referencia mutable al Vec<i32>
    // 3. Esta nueva referencia se pasa a bar()
    // 4. v no se mueve — solo se "presta" temporalmente
}
```

```rust
// Durante el reborrow, v esta "congelado":
//
//   v ──→ Vec<i32> ←── reborrow (temporal)
//   |                      |
//   |  (congelado)         | (activo, usado por bar())
//
// Cuando bar() retorna, el reborrow se destruye y v vuelve a ser valido.

fn foo(v: &mut Vec<i32>) {
    bar(v);   // reborrow #1: v congelado durante bar()
              // bar() retorna → reborrow #1 muere → v desbloqueado

    baz(v);   // reborrow #2: v congelado durante baz()
              // baz() retorna → reborrow #2 muere → v desbloqueado

    v.push(300); // v sigue siendo valido, funciona sin problema
}
```

## Reborrowing implicito

El compilador inserta reborrows automaticamente en varios contextos.
Por eso "simplemente funciona" y normalmente ni te das cuenta:

```rust
// 1. Al pasar &mut T a parametros de funciones:
fn process(data: &mut Vec<i32>) {
    data.push(1);
}

fn main() {
    let mut v = vec![1, 2, 3];
    let r = &mut v;
    process(r);        // compilador inserta: process(&mut *r)
    process(r);        // compilador inserta: process(&mut *r)
    r.push(4);         // r sigue vivo
}
```

```rust
// 2. En llamadas a metodos con &mut self:
let mut s = String::from("hello");
let r = &mut s;
r.push_str(" world");  // reborrow implicito para &mut self
r.push_str("!");        // otro reborrow implicito
println!("{}", r);      // r sigue valido
```

```rust
// 3. Coercion de &mut T a &T (reborrow compartido):
fn print_vec(v: &Vec<i32>) {
    println!("{:?}", v);
}

fn modify_and_print(v: &mut Vec<i32>) {
    v.push(42);
    print_vec(v);   // compilador inserta: print_vec(&*v)
                     // coerce &mut Vec<i32> a &Vec<i32>
    v.push(43);     // v sigue siendo &mut, funciona
}
```

## Reborrowing explicito

A veces necesitas escribir `&mut *v` explicitamente.
Esto ocurre cuando el compilador no puede inferir si
quieres un move o un reborrow:

```rust
// Caso 1: closures con move
fn main() {
    let mut data = vec![10, 20];
    let r = &mut data;

    // Esto mueve r dentro del closure:
    // let closure = move || { r.push(30); };
    // r ya no es valido despues

    // Con reborrow explicito, r sobrevive:
    let reborrow = &mut *r;    // reborrow explicito
    let closure = move || {
        reborrow.push(30);     // el closure mueve reborrow, no r
    };
    closure();
    r.push(40);                // r sigue valido
}
```

```rust
// Caso 2: disambiguacion en contextos genericos
fn generic_fn<T>(val: T) {
    // T podria ser &mut i32 — se moveria
}

fn main() {
    let mut x = 42;
    let r = &mut x;

    generic_fn(&mut *r);  // explicito: quiero reborrow, no move
    // r sigue valido

    generic_fn(r);         // sin reborrow, r se mueve
    // r ya NO es valido aqui
}
```

```rust
// Caso 3: trait que consume self
trait Process {
    fn process(self);
}

impl<'a> Process for &'a mut Vec<i32> {
    fn process(self) {
        self.push(999);
    }
}

fn main() {
    let mut v = vec![1, 2, 3];
    let r = &mut v;

    // r.process() moveria r (consume self)
    (&mut *r).process();  // consume el reborrow, no r
    r.push(4);            // r sigue valido
}
```

## Reborrowing de &T

Las referencias compartidas (`&T`) implementan `Copy`, asi que
reborrowing es menos critico. Copiar `&T` siempre es valido y
no hay riesgo de perder acceso. Pero el reborrowing si ocurre:
`&*r` crea una referencia con un lifetime potencialmente mas corto,
util cuando necesitas desacoplar lifetimes.

```rust
let x = 42;
let r = &x;
let r2 = r;       // COPIA, no move (&T es Copy)
println!("{}", r); // r sigue valido — no hay problema de move
```

## Reborrow vs move

No todo es reborrow. Hay situaciones donde `&mut T` se **mueve**
en vez de reborrowearse. Es importante reconocer la diferencia:

```rust
// REBORROW — pasar a una funcion que espera &mut T:
fn takes_ref(r: &mut i32) {
    *r += 1;
}

fn main() {
    let mut x = 10;
    let r = &mut x;
    takes_ref(r);     // reborrow implicito
    takes_ref(r);     // r sigue vivo, funciona
    println!("{}", r); // 12
}
```

```rust
// MOVE — guardar en un struct:
struct Holder<'a> {
    data: &'a mut i32,
}

fn main() {
    let mut x = 10;
    let r = &mut x;

    let h = Holder { data: r };  // r se MUEVE a h.data
    // println!("{}", r);        // ERROR: r fue movido
    println!("{}", h.data);      // funciona, h es el nuevo dueño
}
```

```rust
// MOVE — closure con move:
fn main() {
    let mut x = 10;
    let r = &mut x;
    let closure = move || { *r += 1; }; // r se movio dentro del closure
    // println!("{}", r);               // ERROR: r fue movido
    closure();
}
```

## Reborrowing y lifetimes

El reborrow siempre tiene un lifetime **mas corto** (o igual) que
el de la referencia original. Esto es lo que hace que sea seguro:

```rust
fn foo<'a>(v: &'a mut Vec<i32>) {
    bar(v);
    //  ^
    //  El compilador crea un reborrow con lifetime 'b donde 'b < 'a
    //
    //  Timeline:
    //  |--- 'a (lifetime de v) --------------------------------|
    //  |         |--- 'b (reborrow) ---|                       |
    //  |         | v congelado         | v libre de nuevo      |
    //  |         | bar() ejecutandose  | bar() retorno         |

    v.push(1); // OK: estamos fuera de 'b, v esta desbloqueado
}
```

```rust
// No puedes tener dos reborrows mutables activos simultaneamente:
fn bad_example(v: &mut Vec<i32>) {
    let r1 = &mut *v;
    let r2 = &mut *v;   // ERROR: v todavia esta prestado a r1
}

// Pero con Non-Lexical Lifetimes (NLL), esto SI funciona:
fn nll_example(v: &mut Vec<i32>) {
    let r1 = &mut *v;
    r1.push(1);          // r1 no se usa mas → lifetime termina (NLL)

    let r2 = &mut *v;   // OK: r1 ya no esta activo
    r2.push(2);

    v.push(3);           // OK: r2 ya no esta activo
}
```

## Implicaciones practicas

### Metodos encadenados sobre &mut self

Puedes llamar multiples metodos `&mut self` en secuencia sobre
la misma variable porque cada llamada es un reborrow independiente:

```rust
fn main() {
    let mut v = vec![1, 2, 3];

    v.push(4);              // reborrow #1, vive durante push()
    v.push(5);              // reborrow #2
    v.retain(|&x| x > 2);  // reborrow #3
    v.sort();               // reborrow #4

    println!("{:?}", v);    // [3, 4, 5]

    // Si &mut self se moviera en vez de reborrowearse,
    // solo podrias llamar UN metodo por variable.
}
```

### Iteracion con &mut

Iterar con `&mut` funciona porque el iterador reborrowa
la referencia mutable. Al terminar el loop, v vuelve a ser usable:

```rust
fn process_all(v: &mut Vec<i32>) {
    for item in v.iter_mut() { *item *= 2; }  // reborrow #1 termina al salir
    v.retain(|&x| x > 4);                      // reborrow #2
    let sum: i32 = v.iter().sum();              // reborrow compartido #3
    v.push(sum);                                // reborrow #4
    println!("{:?}", v);
}
```

### Split borrowing con reborrowing explicito

En structs con multiples campos, a veces necesitas separar
los borrows para convencer al borrow checker. La desestructuracion
crea reborrows independientes de cada campo:

```rust
struct State {
    buffer: Vec<u8>,
    index: Vec<usize>,
}

impl State {
    fn process(&mut self) {
        // self.index.push(self.buffer.len()); // podria no compilar

        // Desestructurar separa los borrows:
        let State { buffer, index } = self;
        index.push(buffer.len());  // OK: borrows independientes
    }
}
```

### Resumen de cuando ocurre reborrow vs move

```rust
// REBORROW (el compilador inserta &mut *r):
//   - Pasar &mut T a un parametro de funcion que espera &mut T
//   - Llamar un metodo con &mut self
//   - Coercion de &mut T a &T
//   - Asignacion a una variable local &mut T (en muchos casos)
//   - Retornar &mut T desde una funcion (con lifetime vinculado)
//
// MOVE (r se consume, ya no es valido):
//   - Guardar &mut T en un campo de struct
//   - Mover &mut T a un closure con move
//   - Pasar &mut T a una funcion generica donde T = &mut U
//   - Cualquier contexto donde el compilador no puede determinar
//     el lifetime del reborrow
```

---

## Ejercicios

### Ejercicio 1 — Identificar reborrows

```rust
// Para cada uso de r, indicar si es REBORROW o MOVE.
// Verificar compilando.

fn takes_mut(v: &mut Vec<i32>) {
    v.push(42);
}

struct Wrapper<'a> {
    inner: &'a mut Vec<i32>,
}

fn main() {
    let mut v = vec![1, 2, 3];
    let r = &mut v;

    takes_mut(r);               // (1) reborrow o move?
    r.push(10);                 // (2) reborrow o move?
    let r2: &mut Vec<i32> = r;  // (3) reborrow o move?
    // r.push(20);              // (4) compilaria? por que?
    let w = Wrapper { inner: r };  // (5) reborrow o move?
    // r.push(30);              // (6) compilaria? por que?
}

// Escribir la respuesta como comentarios y compilar para verificar.
```

### Ejercicio 2 — Reborrow explicito en closures

```rust
// Este codigo no compila. Arreglarlo usando reborrowing explicito
// para que el closure no mueva r:

fn main() {
    let mut data = vec![1, 2, 3, 4, 5];
    let r = &mut data;

    let mut closure = || {
        r.push(6);
    };

    closure();
    r.push(7);  // deberia funcionar despues de llamar al closure
    println!("{:?}", r);
}

// Pista: crear un reborrow explicito antes del closure
// y usar ese reborrow dentro del closure.
```

### Ejercicio 3 — Multiples pasadas con reborrowing

```rust
// Implementar una funcion que reciba &mut Vec<i32> y haga
// tres operaciones secuenciales, cada una usando v:
//
//   1. Duplicar cada elemento (iter_mut)
//   2. Eliminar los menores a 10 (retain)
//   3. Agregar la cantidad de elementos restantes al final (len + push)
//
// Ejemplo: [3, 7, 12, 1] -> [6, 14, 24, 2] -> [14, 24] -> [14, 24, 2]
//
// El punto del ejercicio: verificar que v se puede usar tres veces
// sin problemas. Agregar un comentario en cada paso indicando
// que el reborrow anterior ya termino.
```
