# Lab -- Structs anidados y self-referential

## Objetivo

Practicar structs anidados (struct dentro de struct con acceso de doble
punto), structs self-referential (puntero a si mismo), operaciones basicas
de una linked list (crear, insertar, recorrer, liberar), y forward
declarations para structs que se referencian mutuamente. Al finalizar,
sabras construir estructuras de datos enlazadas en C y gestionar su
memoria correctamente.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `nested.c` | Struct anidado con Date, Address y Employee |
| `selfreferential.c` | Struct Node con puntero a si mismo |
| `linked_list.c` | Linked list con push, append, count y free |
| `forward_decl.c` | Forward declarations y structs mutuamente referenciados |

---

## Parte 1 -- Structs anidados

**Objetivo**: Crear structs que contienen otros structs como miembros y
acceder a los campos internos con doble punto y con punteros.

### Paso 1.1 -- Examinar el codigo fuente

```bash
cat nested.c
```

Observa la estructura:

- `struct Date` tiene 3 campos enteros (year, month, day)
- `struct Address` tiene 2 strings y un entero
- `struct Employee` contiene **dos** `struct Date` y **un** `struct Address`

Esto es anidamiento: Employee no hereda de Date ni Address, los **contiene**
como campos. Los datos de Date y Address viven **dentro** de Employee, no son
punteros a otra parte de la memoria.

### Paso 1.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic nested.c -o nested
./nested
```

Salida esperada:

```
Name: Alice Johnson
Born: 1990-05-15
Hired: 2020-03-01
City: Springfield

Via pointer:
Name: Alice Johnson
Born: 1990-05-15
Address: 742 Evergreen Terrace, Springfield 62704

sizeof(struct Date)     = 12
sizeof(struct Address)  = ~156
sizeof(struct Employee) = ~232
```

### Paso 1.3 -- Analizar el acceso con doble punto

Observa las dos formas de acceso en el codigo:

Acceso directo (variable):
```c
emp.birthday.year       /* doble punto: struct.miembro.campo */
emp.address.city
```

Acceso via puntero:
```c
p->birthday.year        /* flecha + punto: ptr->miembro.campo */
p->address.street
```

La regla es: se usa `->` para el primer nivel (porque `p` es un puntero),
y `.` para los niveles siguientes (porque `birthday` y `address` no son
punteros, son structs embebidos).

### Paso 1.4 -- Analizar los sizeof

Antes de continuar, responde mentalmente:

- `sizeof(struct Date)` es 12 (tres `int` de 4 bytes). Tiene sentido?
- `sizeof(struct Employee)` es mayor que `sizeof(Date) * 2 + sizeof(Address) + 50`?
- Por que podria haber bytes extra? (pista: padding de alineacion)

El sizeof de Employee incluye los sizeof de todos sus miembros internos mas
posible padding. El struct anidado **no** es un puntero -- los datos viven
contiguos en memoria.

### Limpieza de la parte 1

```bash
rm -f nested
```

---

## Parte 2 -- Self-referential structs

**Objetivo**: Entender por que un struct puede contener un puntero a si
mismo pero no una instancia de si mismo, y construir una cadena de nodos
manual.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat selfreferential.c
```

Observa la definicion de `struct Node`:

```c
struct Node {
    int value;
    struct Node *next;  /* pointer to same type */
};
```

Antes de compilar, responde mentalmente:

- Por que `struct Node *next` es valido pero `struct Node next` no lo seria?
- Cuantos bytes ocupa `struct Node` en un sistema de 64 bits? (pista: un
  `int` = 4 bytes, un puntero = 8 bytes, mas padding)

### Paso 2.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic selfreferential.c -o selfreferential
./selfreferential
```

Salida esperada:

```
Manual traversal:
a->value = 10
a->next->value = 20
a->next->next->value = 30

Loop traversal:
10 -> 20 -> 30 -> NULL

Addresses:
a = <addr>, a->next = <addr>
b = <addr>, b->next = <addr>
c = <addr>, c->next = (nil)

sizeof(struct Node) = 16
sizeof(struct Node *) = 8
```

### Paso 2.3 -- Analizar las direcciones

Observa las direcciones impresas:

- `a->next` tiene el mismo valor que `b` -- el puntero `next` de `a` apunta
  a `b`
- `b->next` tiene el mismo valor que `c`
- `c->next` es NULL (fin de la cadena)

Esto forma la cadena: `a -> b -> c -> NULL`

Cada nodo esta en una direccion diferente del heap (alocado con `malloc`).
El campo `next` es lo unico que los conecta.

### Paso 2.4 -- Analizar el sizeof

`sizeof(struct Node)` es 16, no 12. El struct tiene un `int` (4 bytes) y un
puntero (8 bytes) = 12 bytes, pero el compilador agrega 4 bytes de padding
despues del `int` para alinear el puntero a 8 bytes.

`sizeof(struct Node *)` es 8 -- un puntero siempre ocupa 8 bytes en 64 bits,
sin importar a que tipo apunte.

La clave: un struct self-referential funciona porque un puntero tiene tamano
fijo (8 bytes). Si el campo fuera `struct Node next` (sin `*`), el compilador
necesitaria saber el tamano de Node para definir Node -- referencia circular
imposible.

### Limpieza de la parte 2

```bash
rm -f selfreferential
```

---

## Parte 3 -- Linked list basica

**Objetivo**: Implementar y usar una linked list con operaciones de insertar
al inicio (push), insertar al final (append), contar nodos, y liberar
memoria.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat linked_list.c
```

Observa las funciones:

- `node_create(value)` -- aloca un nodo con malloc y retorna el puntero
- `list_push(head, value)` -- inserta al inicio (O(1))
- `list_append(head, value)` -- inserta al final (O(n))
- `list_print(head)` -- recorre e imprime
- `list_count(head)` -- cuenta nodos
- `list_free(head)` -- libera toda la memoria

Antes de compilar, responde mentalmente:

- Por que `list_push` recibe `struct Node **head` (doble puntero) en vez de
  `struct Node *head`?
- Cuando se hace push de 30, 20, 10 en ese orden, cual sera el primer
  elemento de la lista?

### Paso 3.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic linked_list.c -o linked_list
./linked_list
```

Salida esperada:

```
=== Building list with list_push ===
After push 30, 20, 10: 10 -> 20 -> 30 -> NULL
Count: 3

=== Appending with list_append ===
After append 40, 50: 10 -> 20 -> 30 -> 40 -> 50 -> NULL
Count: 5

=== Freeing list ===
List freed. Count: 0

=== Building new list with append only ===
After append 100, 200, 300: 100 -> 200 -> 300 -> NULL
```

### Paso 3.3 -- Verificar la prediccion

El push inserta al **inicio**: push(30) crea `30 -> NULL`, push(20) crea
`20 -> 30 -> NULL`, push(10) crea `10 -> 20 -> 30 -> NULL`. El ultimo
elemento en ser pushed queda primero. Es por eso que la lista resultante
esta en orden inverso al de insercion.

El append inserta al **final**: despues de append(40) y append(50), la lista
es `10 -> 20 -> 30 -> 40 -> 50 -> NULL`. El orden coincide con el de
insercion.

### Paso 3.4 -- Entender el doble puntero

`list_push` necesita `struct Node **head` porque modifica el puntero head
mismo. Si recibiera solo `struct Node *head`, la modificacion seria local a
la funcion y el caller no veria el cambio.

Con doble puntero:
```c
void list_push(struct Node **head, int value) {
    n->next = *head;   /* nuevo nodo apunta al head actual */
    *head = n;          /* head ahora apunta al nuevo nodo */
}
```

La llamada es `list_push(&list, value)` -- se pasa la **direccion** del
puntero `list`.

### Paso 3.5 -- Analizar list_free

Observa como `list_free` guarda `head->next` **antes** de hacer `free(head)`:

```c
void list_free(struct Node *head) {
    while (head) {
        struct Node *next = head->next;  /* guardar ANTES de free */
        free(head);
        head = next;
    }
}
```

Si se hiciera `free(head)` primero y luego `head->next`, se estaria accediendo
a memoria ya liberada (use-after-free), que es comportamiento indefinido.

### Limpieza de la parte 3

```bash
rm -f linked_list
```

---

## Parte 4 -- Forward declaration

**Objetivo**: Usar forward declarations para definir structs que se
referencian mutuamente (Employee apunta a Department, Department apunta a
Employee).

### Paso 4.1 -- Examinar el codigo fuente

```bash
cat forward_decl.c
```

Observa la estructura del archivo:

```c
struct Department;  /* forward declaration */

struct Employee {
    char name[50];
    struct Department *dept;  /* pointer to incomplete type */
};

struct Department {
    char name[50];
    struct Employee *manager;
    int employee_count;
};
```

Antes de compilar, responde mentalmente:

- Que pasaria si se eliminara la linea `struct Department;`?
- Por que `struct Department *dept` funciona con una forward declaration pero
  `struct Department dept` (sin `*`) no funcionaria?

### Paso 4.2 -- Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic forward_decl.c -o forward_decl
./forward_decl
```

Salida esperada:

```
Employee: Alice
  Department: Engineering
  Dept manager: Alice
  Dept size: 15

Employee: Bob
  Department: Marketing
  Dept manager: Bob
  Dept size: 8

Circular check:
alice.dept->manager->name = Alice
alice.dept->manager->dept->name = Engineering
```

### Paso 4.3 -- Verificar la prediccion

Sin la forward declaration, el compilador veria `struct Department *dept`
dentro de `struct Employee` sin saber que `Department` existe. En C, usar
`struct Department *` sin declaracion previa es valido (el compilador asume
que es un incomplete type y crea el tag), pero solo funciona dentro del
mismo scope. La forward declaration explicita es la practica correcta y
evita ambiguedades.

La forward declaration funciona con punteros porque un puntero siempre ocupa
8 bytes (en 64 bits), sin importar el tipo al que apunte. El compilador no
necesita conocer el tamano de `struct Department` para saber el tamano de un
`struct Department *`.

Si fuera `struct Department dept` (sin `*`), el compilador necesitaria
conocer el tamano completo de Department para calcular el sizeof de Employee.
Como Department aun no esta definido, seria un error de "incomplete type".

### Paso 4.4 -- Analizar la referencia circular

Observa la ultima parte de la salida:

```
alice.dept->manager->name = Alice
alice.dept->manager->dept->name = Engineering
```

Se puede navegar la cadena: `alice -> engineering -> alice -> engineering`.
Esto no es recursion infinita -- son punteros que forman un ciclo. El programa
no recorre automaticamente el ciclo, solo accede a los campos que se le piden.

### Limpieza de la parte 4

```bash
rm -f forward_decl
```

---

## Limpieza final

```bash
rm -f nested selfreferential linked_list forward_decl
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  forward_decl.c  linked_list.c  nested.c  selfreferential.c
```

---

## Conceptos reforzados

1. Un struct anidado contiene los datos del struct interno **dentro de si
   mismo** (no como puntero). El acceso usa doble punto: `emp.birthday.year`.
   Con puntero al struct externo, se usa flecha y luego punto:
   `p->birthday.year`.

2. Un struct **no puede** contener una instancia de si mismo (tamano infinito),
   pero **si puede** contener un puntero a si mismo (tamano fijo de 8 bytes
   en 64 bits). Esta es la base de las estructuras enlazadas.

3. Una linked list se construye conectando nodos con punteros `next`. La
   insercion al inicio (push) es O(1), la insercion al final (append) es O(n)
   porque hay que recorrer toda la lista.

4. Las funciones que modifican el head de una lista necesitan recibir un
   **doble puntero** (`struct Node **head`) para que el cambio sea visible
   en el caller.

5. Al liberar una lista, se debe guardar `head->next` **antes** de llamar
   `free(head)`. Acceder a memoria ya liberada es comportamiento indefinido
   (use-after-free).

6. Una forward declaration (`struct X;`) permite declarar punteros a un
   struct antes de definirlo. Es necesaria cuando dos structs se referencian
   mutuamente. Funciona porque un puntero tiene tamano fijo independiente del
   tipo al que apunte.
