# T02 — GDB para punteros

---

## 1. Por qué GDB es necesario para estructuras dinámicas

Valgrind detecta errores de memoria **después** de que ocurren. GDB permite
**pausar el programa en el instante exacto** y examinar el estado de cada
puntero, cada nodo, cada campo.

Para estructuras de datos dinámicas, esto significa:

| Necesidad | Valgrind | GDB |
|-----------|---------|-----|
| Detectar leak | Sí | No |
| Detectar use-after-free | Sí | No directamente |
| Ver el estado de una lista nodo por nodo | No | **Sí** |
| Pausar cuando un campo cambia de valor | No | **Sí** (watchpoint) |
| Recorrer un árbol interactivamente | No | **Sí** |
| Entender por qué `insert` deja la lista corrupta | No | **Sí** |

Valgrind dice *qué* salió mal. GDB permite ver *por qué*.

### Compilar para GDB

```bash
gcc -g -O0 -o program program.c
```

Mismos flags que para Valgrind: `-g` (símbolos de depuración), `-O0` (sin
optimizaciones). Con `-O2`, el compilador reordena instrucciones, elimina
variables, e inlinea funciones — GDB mostraría un programa irreconocible.

---

## 2. Estructura base para los ejemplos

Usaremos la misma lista enlazada del tópico anterior:

```c
// list.h
#ifndef LIST_H
#define LIST_H

typedef struct Node {
    int data;
    struct Node *next;
} Node;

typedef struct {
    Node *head;
    int size;
} List;

List *list_create(void);
void  list_destroy(List *list);
void  list_push_front(List *list, int data);
void  list_push_back(List *list, int data);
void  list_print(const List *list);

#endif
```

```c
// list.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "list.h"

List *list_create(void) {
    List *list = malloc(sizeof(List));
    if (!list) return NULL;
    list->head = NULL;
    list->size = 0;
    return list;
}

void list_destroy(List *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    free(list);
}

void list_push_front(List *list, int data) {
    assert(list);
    Node *node = malloc(sizeof(Node));
    if (!node) return;
    node->data = data;
    node->next = list->head;
    list->head = node;
    list->size++;
}

void list_push_back(List *list, int data) {
    assert(list);
    Node *node = malloc(sizeof(Node));
    if (!node) return;
    node->data = data;
    node->next = NULL;

    if (!list->head) {
        list->head = node;
    } else {
        Node *cur = list->head;
        while (cur->next)
            cur = cur->next;
        cur->next = node;
    }
    list->size++;
}

void list_print(const List *list) {
    if (!list) return;
    for (Node *cur = list->head; cur; cur = cur->next)
        printf("%d -> ", cur->data);
    printf("NULL\n");
}
```

```c
// main.c
#include "list.h"

int main(void) {
    List *list = list_create();
    list_push_back(list, 10);
    list_push_back(list, 20);
    list_push_back(list, 30);
    list_push_back(list, 40);

    list_print(list);
    list_destroy(list);
    return 0;
}
```

---

## 3. Inspeccionar punteros con `print`

### Iniciar GDB y poner breakpoints

```bash
gcc -g -O0 -o test_list list.c main.c
gdb ./test_list
```

```gdb
(gdb) break main
(gdb) run
```

### Navegar paso a paso

```gdb
(gdb) next          # avanza una línea (sin entrar en funciones)
(gdb) next          # list_create() se ejecuta
(gdb) next          # list_push_back(list, 10) se ejecuta
(gdb) next          # list_push_back(list, 20) se ejecuta
(gdb) next          # list_push_back(list, 30) se ejecuta
(gdb) next          # list_push_back(list, 40) se ejecuta
```

### Examinar la lista

```gdb
(gdb) print *list
$1 = {head = 0x5555555596b0, size = 4}
```

Esto muestra la struct `List`: el puntero `head` y el campo `size`. Para ver
el primer nodo:

```gdb
(gdb) print *list->head
$2 = {data = 10, next = 0x5555555596d0}
```

Para seguir la cadena de punteros:

```gdb
(gdb) print *list->head->next
$3 = {data = 20, next = 0x5555555596f0}

(gdb) print *list->head->next->next
$4 = {data = 30, next = 0x555555559710}

(gdb) print *list->head->next->next->next
$5 = {data = 40, next = 0x0}
```

`next = 0x0` es `NULL` — final de la lista.

### Acceder a campos específicos

```gdb
(gdb) print list->head->data
$6 = 10

(gdb) print list->head->next->data
$7 = 20

(gdb) print list->size
$8 = 4
```

### Examinar con formato

```gdb
(gdb) print/x list->head           # dirección en hexadecimal
$9 = 0x5555555596b0

(gdb) print/t list->head->data     # data en binario
$10 = 1010

(gdb) x/4xw list->head             # examinar 4 words de memoria desde head
0x5555555596b0: 0x0000000a  0x00000000  0x555596d0  0x00005555
```

El comando `x` (examine) muestra memoria cruda. `4xw` = 4 unidades, formato
hexadecimal, tamaño word (4 bytes). Los primeros 4 bytes (`0x0000000a` = 10)
son `data`. Los siguientes 8 bytes son `next` (un puntero de 64-bit).

---

## 4. Navegar dentro de funciones con `step`

`next` ejecuta la línea completa (incluyendo llamadas a funciones). `step`
entra **dentro** de la función:

```gdb
(gdb) break main
(gdb) run

# Avanzar hasta list_push_back(list, 20)
(gdb) next
(gdb) next
(gdb) step           # entra en list_push_back
```

Ahora estamos dentro de `list_push_back`:

```gdb
(gdb) print data
$1 = 20

(gdb) next           # node = malloc(sizeof(Node))
(gdb) print *node
$2 = {data = 0, next = 0x0}      # recién alocado, sin inicializar (o calloc'd)

(gdb) next           # node->data = data
(gdb) print node->data
$3 = 20

(gdb) next           # node->next = NULL
(gdb) next           # entra al else (list->head ya existe)

(gdb) print *list->head
$4 = {data = 10, next = 0x0}     # el primer nodo, next es NULL
```

Podemos observar exactamente cómo se recorre la lista para encontrar el final:

```gdb
(gdb) next           # Node *cur = list->head
(gdb) print *cur
$5 = {data = 10, next = 0x0}     # cur apunta al nodo 10

(gdb) next           # while (cur->next) — cur->next es NULL, sale del loop
(gdb) next           # cur->next = node — conecta el nuevo nodo
(gdb) print *cur
$6 = {data = 10, next = 0x5555555596d0}   # ahora cur->next apunta al nodo 20

(gdb) print *cur->next
$7 = {data = 20, next = 0x0}     # el nuevo nodo, correctamente enlazado
```

### `finish` — salir de la función actual

```gdb
(gdb) finish         # ejecuta hasta que list_push_back retorna
                     # vuelve a main
```

### `step` vs `next` vs `finish`

| Comando | Acción |
|---------|--------|
| `next` (n) | Ejecutar la línea actual, sin entrar en funciones |
| `step` (s) | Ejecutar la línea actual, entrando en funciones |
| `finish` | Ejecutar hasta que la función actual retorne |
| `continue` (c) | Continuar hasta el siguiente breakpoint |
| `until` | Continuar hasta una línea mayor que la actual (salir de loop) |

---

## 5. Breakpoints condicionales

### Pausar cuando un nodo tiene cierto valor

```gdb
(gdb) break list_push_back if data == 30
```

GDB solo se detendrá cuando `list_push_back` sea llamado con `data == 30`.
Esto evita detenerse en los push de 10 y 20.

### Pausar en una línea específica

```gdb
(gdb) break list.c:35           # línea 35 de list.c
(gdb) break list.c:35 if cur->data == 20
```

### Listar, habilitar y deshabilitar breakpoints

```gdb
(gdb) info breakpoints          # listar todos los breakpoints
Num  Type       Disp  Enb  Address            What
1    breakpoint keep  y    0x000055555540064a  in main at main.c:4
2    breakpoint keep  y    0x00005555554005d0  in list_push_back at list.c:29 stop only if data == 30

(gdb) disable 1                 # deshabilitar breakpoint 1 (sin eliminarlo)
(gdb) enable 1                  # re-habilitarlo
(gdb) delete 2                  # eliminar breakpoint 2
```

---

## 6. Watchpoints — pausar cuando un valor cambia

Los watchpoints son la herramienta más poderosa para depurar corrupción de
punteros. GDB **pausa el programa en el instante exacto en que un valor cambia**.

### Watchpoint en un campo de un nodo

```gdb
(gdb) break main
(gdb) run
(gdb) next           # list = list_create()
(gdb) next           # list_push_back(list, 10)

# Ahora poner un watchpoint en head
(gdb) watch list->head
Hardware watchpoint 2: list->head
(gdb) continue
```

Cada vez que `list->head` cambie de valor, GDB se detendrá y mostrará:

```
Hardware watchpoint 2: list->head

Old value = 0x5555555596b0
New value = 0x5555555596d0
list_push_front (list=0x555555559690, data=5) at list.c:30
30	    list->head = node;
```

Esto muestra: el valor anterior, el nuevo valor, y la **línea exacta** que
lo modificó.

### Watchpoint en `next` de un nodo específico

Para detectar cuándo se modifica el enlace de un nodo:

```gdb
(gdb) print list->head
$1 = (Node *) 0x5555555596b0

(gdb) watch ((Node *)0x5555555596b0)->next
```

GDB se detendrá cada vez que el campo `next` del nodo en esa dirección cambie.
Esto es invaluable para detectar **corrupción de punteros**: si algún código
modifica `next` inesperadamente, GDB lo atrapa.

### Tipos de watchpoints

| Comando | Cuándo se detiene |
|---------|------------------|
| `watch expr` | Cuando `expr` cambia (escritura) |
| `rwatch expr` | Cuando `expr` se lee |
| `awatch expr` | Cuando `expr` se lee o escribe |

### Limitaciones

Los watchpoints de hardware (los eficientes) son limitados — la mayoría de
CPUs soportan 4 simultáneos. GDB cambia a watchpoints de software (mucho
más lentos) si se excede ese límite.

---

## 7. Recorrer una lista enlazada en GDB

### Método manual: seguir `next`

```gdb
(gdb) set $node = list->head
(gdb) print *$node
$1 = {data = 10, next = 0x5555555596d0}

(gdb) set $node = $node->next
(gdb) print *$node
$2 = {data = 20, next = 0x5555555596f0}

(gdb) set $node = $node->next
(gdb) print *$node
$3 = {data = 30, next = 0x555555559710}

(gdb) set $node = $node->next
(gdb) print *$node
$4 = {data = 40, next = 0x0}
```

`$node` es una **convenience variable** de GDB (prefijo `$`). Persiste
durante la sesión.

### Comando `define` — script para recorrer listas

Para evitar escribir lo mismo repetidamente:

```gdb
(gdb) define walk_list
Type commands for definition of "walk_list":
> set $n = $arg0
> set $i = 0
> while $n
  > printf "[%d] data=%d addr=%p next=%p\n", $i, $n->data, $n, $n->next
  > set $n = $n->next
  > set $i = $i + 1
  > end
> printf "Total: %d nodes\n", $i
> end
```

Uso:

```gdb
(gdb) walk_list list->head
[0] data=10 addr=0x5555555596b0 next=0x5555555596d0
[1] data=20 addr=0x5555555596d0 next=0x5555555596f0
[2] data=30 addr=0x5555555596f0 next=0x555555559710
[3] data=40 addr=0x555555559710 next=0x0
Total: 4 nodes
```

### Archivo `.gdbinit` — comandos automáticos

Guardar el script en un archivo para reutilizar:

```bash
# ~/.gdbinit  o  .gdbinit en el directorio del proyecto
define walk_list
  set $n = $arg0
  set $i = 0
  while $n
    printf "[%d] data=%d addr=%p next=%p\n", $i, $n->data, $n, $n->next
    set $n = $n->next
    set $i = $i + 1
  end
  printf "Total: %d nodes\n", $i
end

define walk_tree
  if $arg0
    walk_tree $arg0->left
    printf "data=%d\n", $arg0->data
    walk_tree $arg0->right
  end
end
```

GDB carga `.gdbinit` automáticamente al iniciar. Para el directorio local:

```bash
echo "set auto-load safe-path /" >> ~/.gdbinit
```

---

## 8. Depurar un árbol binario con GDB

### Estructura del árbol

```c
typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;
```

### Inspeccionar nodos del árbol

```gdb
(gdb) print *root
$1 = {data = 10, left = 0x5555555596d0, right = 0x5555555596f0}

(gdb) print *root->left
$2 = {data = 5, left = 0x555555559710, right = 0x555555559730}

(gdb) print *root->right
$3 = {data = 15, left = 0x0, right = 0x555555559750}

(gdb) print *root->left->left
$4 = {data = 2, left = 0x0, right = 0x0}
```

### Visualizar la estructura

```gdb
(gdb) printf "     %d\n", root->data
(gdb) printf "    / \\\n"
(gdb) printf "   %d  %d\n", root->left->data, root->right->data
```

```
     10
    / \
   5  15
```

### Script para imprimir árbol (in-order)

```gdb
define print_inorder
  if $arg0
    print_inorder $arg0->left
    printf "%d ", $arg0->data
    print_inorder $arg0->right
  end
end
```

```gdb
(gdb) print_inorder root
2 5 7 10 12 15 20
```

---

## 9. Depurar problemas comunes en DS

### Caso 1: `push_back` no enlaza correctamente

Síntoma: la lista solo tiene un nodo después de múltiples push.

```gdb
(gdb) break list_push_back
(gdb) run

# Primer push (data=10)
(gdb) continue
(gdb) finish
(gdb) walk_list list->head
[0] data=10 addr=0x5555555596b0 next=0x0
Total: 1 nodes        # ← OK

# Segundo push (data=20)
(gdb) continue
(gdb) finish
(gdb) walk_list list->head
[0] data=10 addr=0x5555555596b0 next=0x0
Total: 1 nodes        # ← BUG: todavía 1 nodo, el 20 se perdió
```

El bug está en `push_back`: el loop `while (cur->next)` no se conecta
correctamente, o `cur->next = node` no se ejecuta.

Para investigar, usar `step` en la segunda llamada:

```gdb
(gdb) continue         # segundo push_back
(gdb) step             # entrar en push_back
(gdb) next             # hasta cur->next = node
(gdb) print cur        # ¿es el nodo correcto?
(gdb) print node       # ¿existe el nuevo nodo?
(gdb) print cur->next  # ¿se conectó?
```

### Caso 2: segfault en destrucción

```gdb
(gdb) run
Program received signal SIGSEGV, Segmentation fault.
0x00005555554005c8 in list_destroy (list=0x555555559690) at list.c:19
19	        cur = cur->next;
```

GDB muestra la línea exacta del crash. Examinar el estado:

```gdb
(gdb) print cur
$1 = (Node *) 0x5555555596b0
(gdb) print *cur
Cannot access memory at address 0x5555555596b0
```

`cur` apunta a memoria ya liberada. Es el patrón use-after-free:
`free(cur); cur = cur->next;` — `cur` fue liberado una línea antes.

### Caso 3: loop infinito en recorrido

Si la lista está corrupta (circular por error), el programa se cuelga:

```gdb
# Si el programa no responde, presionar Ctrl+C en GDB
^C
Program received signal SIGINT, Interrupt.
0x0000555555400612 in list_print (list=0x555555559690) at list.c:45
45	    for (Node *cur = list->head; cur; cur = cur->next)

(gdb) print *cur
$1 = {data = 20, next = 0x5555555596b0}

(gdb) print *cur->next
$2 = {data = 10, next = 0x5555555596d0}

(gdb) print *cur->next->next
$3 = {data = 20, next = 0x5555555596b0}     # ← CICLO: apunta de vuelta
```

El nodo 20 apunta al nodo 10, que apunta de vuelta al nodo 20. La lista
es circular por error. `walk_list` también entraría en loop infinito —
se puede agregar un límite:

```gdb
define walk_list_safe
  set $n = $arg0
  set $i = 0
  set $max = 100
  while $n && $i < $max
    printf "[%d] data=%d addr=%p next=%p\n", $i, $n->data, $n, $n->next
    set $n = $n->next
    set $i = $i + 1
  end
  if $i >= $max
    printf "WARNING: stopped at %d nodes (possible cycle)\n", $max
  end
  printf "Total: %d nodes\n", $i
end
```

---

## 10. Comandos de referencia rápida

### Navegación

| Comando | Atajo | Acción |
|---------|-------|--------|
| `run` | `r` | Iniciar el programa |
| `next` | `n` | Siguiente línea (sin entrar en funciones) |
| `step` | `s` | Siguiente línea (entrando en funciones) |
| `finish` | `fin` | Ejecutar hasta retornar de la función actual |
| `continue` | `c` | Continuar hasta el siguiente breakpoint |
| `until` | `u` | Continuar hasta una línea mayor (salir de loop) |

### Inspección

| Comando | Acción |
|---------|--------|
| `print expr` (p) | Evaluar e imprimir expresión |
| `print *ptr` | Desreferenciar puntero |
| `print ptr->field` | Acceder a campo de struct |
| `print/x expr` | Imprimir en hexadecimal |
| `x/Nfw addr` | Examinar N unidades de memoria (f=formato, w=tamaño) |
| `display expr` | Imprimir `expr` automáticamente en cada paso |
| `info locals` | Mostrar todas las variables locales |

### Breakpoints y watchpoints

| Comando | Acción |
|---------|--------|
| `break func` | Breakpoint en función |
| `break file:line` | Breakpoint en línea |
| `break ... if cond` | Breakpoint condicional |
| `watch expr` | Watchpoint (pausar en escritura) |
| `rwatch expr` | Watchpoint (pausar en lectura) |
| `info breakpoints` | Listar todos |
| `delete N` | Eliminar breakpoint N |
| `disable/enable N` | Deshabilitar/habilitar breakpoint N |

### Pila de llamadas

| Comando | Acción |
|---------|--------|
| `backtrace` (bt) | Mostrar la pila de llamadas completa |
| `frame N` | Cambiar al frame N de la pila |
| `up` / `down` | Moverse entre frames |

`backtrace` es esencial cuando hay un crash — muestra la cadena completa
de llamadas que llevó al error:

```gdb
(gdb) bt
#0  list_destroy (list=0x555555559690) at list.c:19
#1  0x000055555540068a in main () at main.c:10
```

---

## Ejercicios

### Ejercicio 1 — Inspeccionar una lista

Compila y ejecuta en GDB:

```c
#include "list.h"

int main(void) {
    List *list = list_create();
    list_push_back(list, 100);
    list_push_back(list, 200);
    list_push_back(list, 300);
    list_print(list);
    list_destroy(list);
    return 0;
}
```

Usa `break main`, avanza con `next` hasta después del tercer push, y responde:

1. ¿Cuál es el valor de `list->head->data`?
2. ¿Cuál es el valor de `list->head->next->next->data`?
3. ¿Cuál es el valor de `list->size`?
4. ¿Qué pasa si haces `print *list->head->next->next->next`?

**Prediccion**: ¿El último `print` mostrará `{data=0, next=0x0}` o dará error?

<details><summary>Respuesta</summary>

1. `list->head->data` = **100** (primer nodo)
2. `list->head->next->next->data` = **300** (tercer nodo)
3. `list->size` = **3**
4. `print *list->head->next->next->next` produce:

```
Cannot access memory at address 0x0
```

Porque `list->head->next->next->next` es `NULL` (el tercer nodo es el
último, su `next` es `0x0`). GDB no puede desreferenciar un puntero nulo.

Para verificar sin desreferenciar:
```gdb
(gdb) print list->head->next->next->next
$1 = (Node *) 0x0
```

Esto muestra que es `NULL` sin intentar leer la memoria.

</details>

---

### Ejercicio 2 — Usar `step` para entender `push_back`

Con breakpoint en `main`, avanza con `next` hasta la segunda llamada a
`list_push_back` (la que inserta 200). Usa `step` para entrar en la función.

1. ¿Qué valor tiene `data` al entrar?
2. Avanza hasta el `while (cur->next)`. ¿Cuántas iteraciones hace?
3. ¿A qué nodo apunta `cur` cuando sale del while?

**Prediccion**: Para insertar el segundo elemento, ¿el while hace 0 o 1
iteraciones?

<details><summary>Respuesta</summary>

1. `data` = **200**
2. El while hace **0 iteraciones**: `cur` empieza en `list->head` (nodo 100),
   y `cur->next` es `NULL` → la condición es falsa inmediatamente.
3. `cur` apunta al nodo 100 (el único nodo existente).

Después de `cur->next = node`, el nodo 100 queda enlazado al nodo 200.

Para el tercer push (300), el while haría **1 iteración**: `cur` empieza
en 100, `cur->next` es nodo 200 (no NULL), avanza, `cur` ahora es 200,
`cur->next` es NULL → sale.

En general, `push_back` recorre $n-1$ nodos para insertar al final — es
$O(n)$. Una mejora sería mantener un puntero `tail` en la struct `List`.

</details>

---

### Ejercicio 3 — Breakpoint condicional

Modifica `main` para insertar los números 1 a 20. Pon un breakpoint
condicional que pare solo cuando se inserte el número 15:

```c
for (int i = 1; i <= 20; i++)
    list_push_back(list, i);
```

Escribe el comando GDB necesario y verifica que el breakpoint solo se
active una vez.

**Prediccion**: ¿Cómo se escribe el breakpoint condicional?

<details><summary>Respuesta</summary>

```gdb
(gdb) break list_push_back if data == 15
Breakpoint 1 at 0x...: file list.c, line 29.
(gdb) run
```

GDB se detiene solo una vez, cuando `data == 15`. Verificar:

```gdb
Breakpoint 1, list_push_back (list=0x..., data=15) at list.c:29
(gdb) print data
$1 = 15
(gdb) print list->size
$2 = 14         # ya se insertaron 14 nodos antes
```

Para verificar que la lista tiene los primeros 14 nodos:

```gdb
(gdb) walk_list list->head
[0] data=1 addr=... next=...
[1] data=2 addr=... next=...
...
[13] data=14 addr=... next=0x0
Total: 14 nodes
```

Alternativa: breakpoint en una línea con condición más compleja:

```gdb
(gdb) break list.c:35 if cur->data == 10 && node->data == 15
```

</details>

---

### Ejercicio 4 — Watchpoint en `head`

Crea una lista vacía, pon un watchpoint en `list->head`, y luego haz dos
`push_front`. Describe qué muestra GDB en cada pausa.

**Prediccion**: ¿Cuántas veces se detendrá el watchpoint? ¿Qué valores
mostrará?

<details><summary>Respuesta</summary>

Se detendrá **2 veces** (una por cada push_front que modifica `list->head`):

```gdb
(gdb) break main
(gdb) run
(gdb) next              # list = list_create()
(gdb) watch list->head
Hardware watchpoint 2: list->head
(gdb) continue
```

**Primera pausa** (push_front 10):
```
Hardware watchpoint 2: list->head
Old value = (Node *) 0x0
New value = (Node *) 0x5555555596b0
list_push_front (list=0x..., data=10) at list.c:30
30	    list->head = node;
```

`head` cambió de `NULL` a la dirección del primer nodo.

**Segunda pausa** (push_front 20):
```
Hardware watchpoint 2: list->head
Old value = (Node *) 0x5555555596b0
New value = (Node *) 0x5555555596d0
list_push_front (list=0x..., data=20) at list.c:30
30	    list->head = node;
```

`head` ahora apunta al nuevo nodo (20), que a su vez apunta al anterior (10).

Con `push_back`, `list->head` solo se modifica una vez (en el primer push
cuando la lista está vacía), porque los demás nodos se agregan al final
sin tocar `head`.

</details>

---

### Ejercicio 5 — Detectar ciclo con GDB

Dado este código que introduce un ciclo accidental:

```c
int main(void) {
    List *list = list_create();
    list_push_back(list, 10);
    list_push_back(list, 20);
    list_push_back(list, 30);

    // Bug: crear un ciclo
    list->head->next->next->next = list->head;   // 30->next = head (10)

    list_print(list);   // loop infinito
    list_destroy(list);
    return 0;
}
```

Sin modificar el código, usa GDB para:
1. Poner un breakpoint antes de `list_print`
2. Verificar que hay un ciclo examinando los punteros
3. Identificar cuál nodo cierra el ciclo

**Prediccion**: ¿Cómo verificarías que el tercer nodo apunta de vuelta
al primero?

<details><summary>Respuesta</summary>

```gdb
(gdb) break main.c:10          # línea antes de list_print
(gdb) run

# Examinar la cadena de punteros
(gdb) print list->head
$1 = (Node *) 0x5555555596b0

(gdb) print list->head->next
$2 = (Node *) 0x5555555596d0

(gdb) print list->head->next->next
$3 = (Node *) 0x5555555596f0

(gdb) print list->head->next->next->next
$4 = (Node *) 0x5555555596b0      # ← MISMA dirección que $1 (head)
```

La dirección `0x5555555596b0` aparece tanto en `$1` como en `$4`.
Esto confirma el ciclo: el tercer nodo (30) apunta de vuelta al primero (10).

Para verificar de forma explícita:

```gdb
(gdb) print list->head->next->next->next == list->head
$5 = 1          # true — son la misma dirección
```

Ahora se puede corregir el ciclo desde GDB sin modificar el código:

```gdb
(gdb) set list->head->next->next->next = 0
(gdb) continue
```

Esto corrige `next` del nodo 30 a `NULL`, permitiendo que `list_print`
y `list_destroy` funcionen correctamente.

</details>

---

### Ejercicio 6 — Backtrace en un segfault

Ejecuta este programa con GDB y usa `backtrace` para encontrar la causa
del segfault:

```c
void process_node(Node *node) {
    printf("processing %d\n", node->data);
}

void process_list(List *list) {
    Node *cur = list->head;
    while (cur) {
        process_node(cur);
        cur = cur->next;
    }
    // Bug: intenta procesar un nodo más
    process_node(cur);   // cur es NULL aquí
}

int main(void) {
    List *list = list_create();
    list_push_back(list, 10);
    list_push_back(list, 20);
    process_list(list);
    list_destroy(list);
    return 0;
}
```

**Prediccion**: ¿Qué mostrará `bt` después del crash?

<details><summary>Respuesta</summary>

```gdb
(gdb) run
processing 10
processing 20
Program received signal SIGSEGV, Segmentation fault.
0x0000555555400590 in process_node (node=0x0) at main.c:4
4	    printf("processing %d\n", node->data);

(gdb) bt
#0  0x0000555555400590 in process_node (node=0x0) at main.c:4
#1  0x00005555554005e0 in process_list (list=0x555555559690) at main.c:12
#2  0x000055555540063a in main () at main.c:18
```

El backtrace muestra la cadena completa:
- **Frame #0**: `process_node` recibió `node=0x0` (NULL) → crash
- **Frame #1**: `process_list` llamó a `process_node(cur)` en línea 12,
  cuando `cur` ya era NULL (después del while)
- **Frame #2**: `main` llamó a `process_list` en línea 18

Para inspeccionar el estado en `process_list`:

```gdb
(gdb) frame 1
(gdb) print cur
$1 = (Node *) 0x0        # cur es NULL — el while terminó correctamente
(gdb) print list->size
$2 = 2
```

El bug es claro: la llamada `process_node(cur)` fuera del while debería
eliminarse o protegerse con `if (cur)`.

</details>

---

### Ejercicio 7 — Script GDB para verificar lista ordenada

Escribe un comando `define` en GDB que verifique si una lista enlazada
está ordenada de menor a mayor. Debe imprimir "SORTED" o "NOT SORTED"
con el par que viola el orden.

**Prediccion**: ¿Qué necesitas comparar en cada iteración del script?

<details><summary>Respuesta</summary>

```gdb
define check_sorted
  set $cur = $arg0
  set $sorted = 1
  if $cur
    while $cur->next
      if $cur->data > $cur->next->data
        printf "NOT SORTED: %d > %d\n", $cur->data, $cur->next->data
        set $sorted = 0
      end
      set $cur = $cur->next
    end
  end
  if $sorted
    printf "SORTED\n"
  end
end
```

Uso:

```gdb
(gdb) check_sorted list->head
SORTED

# Si la lista es [10, 5, 20]:
(gdb) check_sorted list->head
NOT SORTED: 10 > 5
```

En cada iteración se compara `cur->data` con `cur->next->data`. Si
`cur->data > cur->next->data`, la lista no está ordenada.

Este tipo de scripts es útil como **verificación de invariantes** durante
depuración: si un BST insert debe mantener el orden, se ejecuta
`check_sorted` después de cada insert para detectar cuándo se rompe.

</details>

---

### Ejercicio 8 — Watchpoint en `size`

Pon un watchpoint en `list->size` y ejecuta este programa. ¿Cuántas veces
se detiene? ¿Hay alguna inconsistencia?

```c
int main(void) {
    List *list = list_create();
    list_push_front(list, 10);
    list_push_front(list, 20);
    list_push_front(list, 30);

    // Bug: remover sin decrementar size
    Node *old = list->head;
    list->head = old->next;
    free(old);
    // list->size-- olvidado

    list_push_front(list, 40);
    list_print(list);
    list_destroy(list);
    return 0;
}
```

**Prediccion**: Después de todas las operaciones, ¿qué valores tendrán
`list->size` y la cantidad real de nodos?

<details><summary>Respuesta</summary>

El watchpoint en `list->size` se detiene **4 veces**:

| Evento | Old value | New value | Función |
|--------|----------|----------|---------|
| push_front(10) | 0 | 1 | `list_push_front` |
| push_front(20) | 1 | 2 | `list_push_front` |
| push_front(30) | 2 | 3 | `list_push_front` |
| push_front(40) | 3 | 4 | `list_push_front` |

El watchpoint **no se detiene** durante la eliminación manual porque
`list->size--` fue olvidado. `size` no cambia → el watchpoint no se activa.

Estado final:
- `list->size` = **4** (incorrecto)
- Nodos reales = **3** (40 → 20 → 10, el nodo 30 fue eliminado)

Para detectar esta inconsistencia con GDB:

```gdb
(gdb) walk_list list->head
[0] data=40 addr=...
[1] data=20 addr=...
[2] data=10 addr=...
Total: 3 nodes

(gdb) print list->size
$1 = 4                    # ← inconsistente con los 3 nodos reales
```

La lección: un watchpoint en `size` permite monitorear **todas las
modificaciones legítimas**, pero su ausencia (no se activó durante la
eliminación manual) es en sí misma la evidencia del bug.

</details>

---

### Ejercicio 9 — `display` para seguimiento automático

El comando `display` imprime una expresión automáticamente en cada paso.
Usa `display` para monitorear `cur`, `cur->data` y `cur->next` mientras
se ejecuta `list_destroy`:

```gdb
(gdb) break list_destroy
(gdb) run
(gdb) display cur
(gdb) display cur->data
(gdb) display cur->next
(gdb) next
(gdb) next
...
```

**Prediccion**: ¿Qué pasa con `display cur->data` cuando `cur` llega a NULL?

<details><summary>Respuesta</summary>

Cuando `cur` es `NULL`, GDB no puede evaluar `cur->data`:

```gdb
1: cur = (Node *) 0x0
2: cur->data = <error: Cannot access memory at address 0x4>
3: cur->next = <error: Cannot access memory at address 0x8>
```

GDB no crashea — simplemente muestra `<error:...>` para las expresiones
que no puede evaluar. Esto es seguro.

Para evitar los mensajes de error, se puede usar `display` condicional
con un script, o simplemente ignorarlos. Los displays con error no
afectan la ejecución del programa.

Para limpiar los displays:

```gdb
(gdb) info display           # listar todos los displays activos
(gdb) undisplay 2            # eliminar display #2
(gdb) undisplay               # eliminar todos
```

`display` es extremadamente útil para recorridos de listas/árboles:
permite ver cómo `cur` avanza nodo por nodo sin tener que escribir
`print` en cada paso.

</details>

---

### Ejercicio 10 — GDB vs Valgrind: elegir la herramienta

Para cada situación, indica si usarías GDB, Valgrind, o ambos:

```
a) El programa crashea con segfault al destruir un árbol
b) El programa termina OK pero usas 100 MB para 1000 enteros
c) Un BST insert deja el árbol desordenado
d) list_remove elimina el nodo correcto pero después list_print crashea
e) El programa funciona pero Valgrind reporta "Conditional jump depends
   on uninitialised value"
f) push_back tarda 10 segundos para 1000 elementos
```

**Prediccion**: ¿Hay algún caso donde necesites las dos herramientas?

<details><summary>Respuesta</summary>

| Situación | Herramienta | Razón |
|-----------|------------|-------|
| a) Segfault en destroy | **GDB** primero | `bt` + inspeccionar nodo que causa el crash |
| b) 100 MB para 1000 ints | **Valgrind** (massif) | `valgrind --tool=massif` para ver alocaciones |
| c) BST desordenado | **GDB** | Breakpoint en insert, `check_sorted` después de cada insert |
| d) Remove OK, print crashea | **Ambos** | Valgrind detecta use-after-free; GDB para ver el estado de la lista después de remove |
| e) Valor no inicializado | **Valgrind** | `--track-origins=yes` para encontrar el malloc sin inicializar |
| f) push_back lento | **GDB** | Breakpoint + `until` para ver cuántas iteraciones hace el loop (¿lista circular?) |

**Caso d** es el que más se beneficia de ambas herramientas:

1. **Valgrind** primero: confirma que hay use-after-free o dangling pointer
2. **GDB** después: poner breakpoint en `list_remove`, examinar la lista
   antes y después, verificar que `prev->next` se reconectó correctamente

El flujo típico de depuración de DS es:
```
1. Valgrind → ¿hay errores de memoria?
2. Si sí → GDB para encontrar exactamente dónde y por qué
3. Corregir → Valgrind de nuevo para confirmar "0 errors"
```

</details>

Limpieza:

```bash
rm -f test_list tree_test
```
