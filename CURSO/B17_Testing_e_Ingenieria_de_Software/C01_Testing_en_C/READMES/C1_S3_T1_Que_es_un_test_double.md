# Que es un test double — fake, stub, mock, spy — diferencias reales, no solo nomenclatura

## 1. El problema fundamental

Cuando el codigo que quieres testear depende de algo externo (base de datos, red, sistema de archivos, hardware, reloj, servicio web), tienes un problema:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     EL PROBLEMA DE LAS DEPENDENCIAS                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Tu codigo bajo test:                                                           │
│  ┌─────────────────────┐                                                        │
│  │   user_service.c    │                                                        │
│  │                     │                                                        │
│  │  create_user() ─────┼──▶ database.c ──▶ PostgreSQL (servidor externo)       │
│  │                     │                                                        │
│  │  send_welcome() ────┼──▶ email.c ──▶ servidor SMTP (red)                    │
│  │                     │                                                        │
│  │  log_event() ───────┼──▶ logger.c ──▶ /var/log/app.log (filesystem)         │
│  │                     │                                                        │
│  │  get_timestamp() ───┼──▶ time() ──▶ reloj del sistema (no determinista)     │
│  │                     │                                                        │
│  └─────────────────────┘                                                        │
│                                                                                  │
│  Para testear create_user() necesitas:                                          │
│  • Un servidor PostgreSQL corriendo                                             │
│  • Una base de datos de test creada                                             │
│  • Credenciales configuradas                                                    │
│  • Red disponible                                                               │
│  • Datos de prueba insertados                                                   │
│  • Cleanup despues de cada test                                                 │
│                                                                                  │
│  Esto es un TEST DE INTEGRACION. Es valido, pero:                              │
│  • Lento (conexion de red, I/O de disco)                                        │
│  • Fragil (falla si el servidor esta caido)                                     │
│  • No determinista (resultado depende del estado externo)                       │
│  • Dificil de configurar en CI                                                  │
│  • Imposible de paralelizar sin aislamiento                                     │
│                                                                                  │
│  ¿Que pasa si solo quieres verificar la LOGICA de create_user()?               │
│  • ¿Valida correctamente el email?                                              │
│  • ¿Hashea la contraseña antes de guardar?                                      │
│  • ¿Retorna el error correcto si el usuario ya existe?                          │
│                                                                                  │
│  Para eso necesitas REEMPLAZAR las dependencias con algo controlado.            │
│  Ese "algo controlado" es un TEST DOUBLE.                                       │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Definicion — test double

El termino **test double** viene de la industria del cine: un **stunt double** (doble de riesgo) reemplaza al actor real en escenas peligrosas. Un test double reemplaza una dependencia real en escenas de testing.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                                                                  │
│  TEST DOUBLE = cualquier objeto/modulo/funcion que reemplaza una               │
│  dependencia real durante el testing.                                            │
│                                                                                  │
│  Analogia del cine:                                                             │
│                                                                                  │
│  Actor real (dependencia real)     Doble (test double)                          │
│  ┌──────────────────────────┐     ┌──────────────────────────┐                  │
│  │ PostgreSQL database      │     │ Array en memoria         │                  │
│  │ SMTP email server        │     │ Funcion que retorna OK   │                  │
│  │ /var/log/app.log         │     │ Buffer en memoria        │                  │
│  │ time() del sistema       │     │ Funcion que retorna 1000 │                  │
│  │ HTTP API externa         │     │ Respuesta hardcodeada    │                  │
│  │ Hardware sensor          │     │ Valor fijo simulado      │                  │
│  └──────────────────────────┘     └──────────────────────────┘                  │
│                                                                                  │
│  El codigo bajo test NO SABE que esta hablando con un doble.                   │
│  Desde su perspectiva, la interfaz es identica.                                 │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

El termino fue popularizado por **Gerard Meszaros** en su libro *xUnit Test Patterns* (2007). Es el termino paraguas. Dentro de test doubles hay **cinco tipos** con roles diferentes.

---

## 3. Los cinco tipos de test doubles

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     TAXONOMIA DE TEST DOUBLES                                    │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Menos comportamiento ◀──────────────────────────────▶ Mas comportamiento       │
│                                                                                  │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐              │
│  │  DUMMY  │  │  STUB   │  │  FAKE   │  │  SPY    │  │  MOCK   │              │
│  │         │  │         │  │         │  │         │  │         │              │
│  │ Relleno │  │ Retorna │  │ Imple-  │  │ Graba   │  │ Verifica│              │
│  │ para    │  │ valores │  │ menta-  │  │ llamadas│  │ expec-  │              │
│  │ compilar│  │ fijos   │  │ cion    │  │ para    │  │ tativas │              │
│  │         │  │         │  │ simple  │  │ inspec- │  │ de      │              │
│  │         │  │         │  │ funcio- │  │ cion    │  │ interac-│              │
│  │         │  │         │  │ nal     │  │ despues │  │ cion    │              │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘              │
│                                                                                  │
│  Dummy: existe para que el codigo compile/ejecute, nunca se usa realmente       │
│  Stub:  retorna respuestas pre-configuradas, sin logica                         │
│  Fake:  tiene logica funcional simplificada (DB en memoria, etc.)               │
│  Spy:   registra las interacciones para verificar despues                        │
│  Mock:  tiene expectativas PRE-DEFINIDAS que se verifican automaticamente       │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Dummy — relleno sin comportamiento

### 4.1 Definicion

Un **dummy** es un objeto que se pasa como argumento pero **nunca se usa realmente**. Existe solo para satisfacer la firma de una funcion. Si el codigo intenta usar el dummy, deberia fallar.

### 4.2 Ejemplo en C

```c
// Codigo de produccion:
typedef struct Logger Logger;
int logger_write(Logger *log, const char *msg);

// user_service necesita un Logger, pero create_user() solo lo usa
// para logging opcional, y en este test no nos importa el logging.

typedef struct {
    int id;
    char name[64];
    char email[128];
} User;

int create_user(const char *name, const char *email, Logger *log);
```

```c
// Test con dummy:
static void test_create_user_valid_data(void) {
    // Logger *dummy = NULL es el dummy mas simple en C
    // create_user no deberia crashear si log es NULL
    // (el codigo debe hacer: if (log) logger_write(log, ...))
    int result = create_user("Alice", "alice@test.com", NULL);
    assert(result == 0);
}
```

En C, el dummy mas comun es simplemente **NULL**. El codigo debe estar escrito para tolerar NULL en argumentos opcionales. Si no lo tolera, puedes crear un struct vacio:

```c
// Dummy mas sofisticado: un Logger que existe pero no hace nada
static int dummy_write(Logger *log, const char *msg) {
    (void)log;
    (void)msg;
    return 0;  // siempre OK, no hace nada
}

// En el test:
Logger dummy_logger = { .write = dummy_write };
int result = create_user("Alice", "alice@test.com", &dummy_logger);
```

### 4.3 Cuando usar dummy

- Cuando una funcion requiere un parametro que el test no ejercita
- Cuando el parametro es obligatorio pero irrelevante para el test
- Cuando quieres que el test falle (assert, SEGFAULT) si el codigo usa el dummy inesperadamente

### 4.4 Dummy vs no pasar nada

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  ¿Por que no simplemente pasar NULL siempre?                                    │
│                                                                                  │
│  1. NULL puede tener significado semantico (ej: "sin logger" vs "error")       │
│  2. El codigo puede no validar NULL y crashear                                  │
│  3. Un dummy explicito documenta la intencion: "no me importa este parametro"   │
│  4. En lenguajes con tipos estrictos, NULL no compila (C es permisivo aqui)     │
│                                                                                  │
│  En C, NULL como dummy es la norma. En C++ o Java, el dummy suele              │
│  ser un objeto minimo que satisface la interfaz.                                │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Stub — respuestas pre-configuradas

### 5.1 Definicion

Un **stub** retorna **respuestas pre-configuradas** cuando se le llama. No tiene logica interna. Solo devuelve lo que le dijiste que devolviera. No verifica como fue llamado.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  STUB                                                                            │
│                                                                                  │
│  Pregunta que responde:  "Si la dependencia retorna X, ¿mi codigo hace Y?"     │
│  Lo que NO responde:     "¿Mi codigo llamo a la dependencia correctamente?"     │
│                                                                                  │
│  Ejemplo mental:                                                                │
│  ┌──────────────────────────────────────────────────────────┐                    │
│  │  test: "Si la base de datos retorna 'usuario existe',   │                    │
│  │         ¿create_user() retorna error DUPLICATE?"         │                    │
│  │                                                          │                    │
│  │  No me importa QUE query se envio a la DB.              │                    │
│  │  Solo me importa como reacciona create_user() al         │                    │
│  │  resultado que la DB retorna.                            │                    │
│  └──────────────────────────────────────────────────────────┘                    │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Ejemplo en C — stub de base de datos

```c
// database.h — interfaz real
typedef struct Database Database;

typedef struct {
    int id;
    char name[64];
    char email[128];
} UserRecord;

// Buscar usuario por email. Retorna 0 si encontrado, -1 si no.
int db_find_user_by_email(Database *db, const char *email, UserRecord *out);

// Insertar usuario. Retorna el ID asignado, o -1 si error.
int db_insert_user(Database *db, const UserRecord *user);
```

```c
// stub_database.c — stub para tests
#include "database.h"
#include <string.h>

// Estado del stub: que queremos que retorne
static int stub_find_result = -1;         // -1 = no encontrado
static UserRecord stub_find_record = {0};
static int stub_insert_result = 1;        // ID asignado

// Configurar el stub antes de cada test
void stub_db_set_find_result(int result, const UserRecord *record) {
    stub_find_result = result;
    if (record) {
        stub_find_record = *record;
    }
}

void stub_db_set_insert_result(int result) {
    stub_insert_result = result;
}

void stub_db_reset(void) {
    stub_find_result = -1;
    memset(&stub_find_record, 0, sizeof(stub_find_record));
    stub_insert_result = 1;
}

// Implementacion del stub — ignora db, retorna valores pre-configurados
int db_find_user_by_email(Database *db, const char *email, UserRecord *out) {
    (void)db;
    (void)email;  // El stub ignora los argumentos
    if (stub_find_result == 0 && out) {
        *out = stub_find_record;
    }
    return stub_find_result;
}

int db_insert_user(Database *db, const UserRecord *user) {
    (void)db;
    (void)user;   // El stub ignora los argumentos
    return stub_insert_result;
}
```

```c
// test_user_service.c — usando el stub
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "user_service.h"

// Declaraciones del stub
extern void stub_db_set_find_result(int result, const UserRecord *record);
extern void stub_db_set_insert_result(int result);
extern void stub_db_reset(void);

static void test_create_user_success(void) {
    stub_db_reset();
    // Configurar stub: db_find retorna -1 (usuario no existe)
    stub_db_set_find_result(-1, NULL);
    // Configurar stub: db_insert retorna 42 (nuevo ID)
    stub_db_set_insert_result(42);

    int user_id = user_service_create("Alice", "alice@test.com", NULL);

    assert(user_id == 42);
    printf("  PASS: create user success\n");
}

static void test_create_user_duplicate_email(void) {
    stub_db_reset();
    // Configurar stub: db_find retorna 0 (usuario YA existe)
    UserRecord existing = {.id = 1, .name = "Bob", .email = "bob@test.com"};
    stub_db_set_find_result(0, &existing);

    int user_id = user_service_create("Bob", "bob@test.com", NULL);

    assert(user_id == -1);  // error: duplicado
    printf("  PASS: create user duplicate email rejected\n");
}

static void test_create_user_db_insert_fails(void) {
    stub_db_reset();
    stub_db_set_find_result(-1, NULL);
    // Configurar stub: db_insert retorna -1 (error de DB)
    stub_db_set_insert_result(-1);

    int user_id = user_service_create("Charlie", "charlie@test.com", NULL);

    assert(user_id == -1);  // error de insercion
    printf("  PASS: create user handles DB error\n");
}

int main(void) {
    printf("=== user_service tests (con stubs) ===\n");
    test_create_user_success();
    test_create_user_duplicate_email();
    test_create_user_db_insert_fails();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

```bash
# Compilacion: linkear con stub en lugar de la DB real
gcc -o test_user_service test_user_service.c user_service.c stub_database.c \
    -I include
# Nota: NO linkeamos database.c (la implementacion real)
```

### 5.3 Stub de tiempo

```c
// Stub de time() — uno de los mas comunes
// El problema: time() retorna la hora real, haciendo el test no-determinista

// Opcion 1: wrapper function
// production code usa get_current_time() en vez de time()
time_t get_current_time(void) {
    return time(NULL);
}

// stub:
static time_t stub_time_value = 0;

void stub_set_time(time_t t) {
    stub_time_value = t;
}

time_t get_current_time(void) {
    return stub_time_value;
}

// Test:
static void test_token_expiration(void) {
    stub_set_time(1000);  // "ahora" es segundo 1000

    Token *t = token_create("user123", 3600);  // expira en 1 hora
    assert(token_is_valid(t, 1000));    // valido ahora
    assert(token_is_valid(t, 4599));    // valido a los 3599s
    assert(!token_is_valid(t, 4601));   // invalido despues de 3600s

    token_free(t);
    printf("  PASS: token expiration\n");
}
```

### 5.4 Caracteristicas del stub

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  STUB — PROPIEDADES                                                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ✓ Retorna valores pre-configurados                                             │
│  ✓ No tiene logica interna                                                      │
│  ✓ No verifica como fue llamado                                                 │
│  ✓ No verifica con que argumentos                                               │
│  ✓ No verifica cuantas veces fue llamado                                        │
│  ✓ Es simple de implementar                                                     │
│                                                                                  │
│  ✗ No detecta si el codigo llamo al metodo equivocado                          │
│  ✗ No detecta si los argumentos fueron incorrectos                              │
│  ✗ No detecta si se llamo demasiadas o pocas veces                             │
│                                                                                  │
│  Uso principal: controlar el INPUT al codigo bajo test                          │
│  (que ve el codigo cuando llama a la dependencia)                               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Fake — implementacion simplificada pero funcional

### 6.1 Definicion

Un **fake** tiene una implementacion **real pero simplificada**. A diferencia del stub (que solo retorna valores fijos), el fake tiene **logica interna** que funciona. La diferencia con la implementacion real es que el fake toma atajos que lo hacen inadecuado para produccion.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  FAKE vs STUB                                                                   │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Stub de DB:                         Fake de DB:                                │
│  ┌────────────────────────────┐     ┌────────────────────────────┐              │
│  │ db_find → retorna -1      │     │ db_find → busca en array   │              │
│  │ db_insert → retorna 42    │     │ db_insert → agrega a array │              │
│  │ (valores fijos)           │     │ db_delete → borra de array │              │
│  │ (sin estado)              │     │ (tiene estado interno)     │              │
│  └────────────────────────────┘     └────────────────────────────┘              │
│                                                                                  │
│  El stub retorna siempre lo mismo sin importar que le pidas.                    │
│  El fake FUNCIONA: si insertas un usuario, luego lo puedes buscar.             │
│                                                                                  │
│  Implementacion real      Fake                          Stub                    │
│  ┌──────────────────┐    ┌──────────────────────┐      ┌─────────────┐          │
│  │ PostgreSQL       │    │ Array en memoria     │      │ return 42   │          │
│  │ - Disco          │    │ - Sin disco          │      │ (siempre)   │          │
│  │ - Red            │    │ - Sin red            │      │             │          │
│  │ - SQL parsing    │    │ - Linear scan        │      │             │          │
│  │ - Transactions   │    │ - Sin transactions   │      │             │          │
│  │ - ACID           │    │ - No durable         │      │             │          │
│  └──────────────────┘    └──────────────────────┘      └─────────────┘          │
│  Completo, lento         Funcional, rapido              Tonto, rapido           │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Ejemplo en C — fake database en memoria

```c
// fake_database.h
#ifndef FAKE_DATABASE_H
#define FAKE_DATABASE_H

#include "database.h"

#define FAKE_DB_MAX_RECORDS 100

typedef struct {
    UserRecord records[FAKE_DB_MAX_RECORDS];
    int count;
    int next_id;
} FakeDatabase;

void fake_db_init(FakeDatabase *fdb);
void fake_db_reset(FakeDatabase *fdb);

// Para que user_service.c pueda usar db_find_user_by_email y db_insert_user
// sin saber que es un fake, la FakeDatabase se expone via puntero global:
void fake_db_install(FakeDatabase *fdb);

#endif
```

```c
// fake_database.c
#include "fake_database.h"
#include <string.h>
#include <stdio.h>

static FakeDatabase *current_fdb = NULL;

void fake_db_init(FakeDatabase *fdb) {
    memset(fdb, 0, sizeof(*fdb));
    fdb->next_id = 1;
}

void fake_db_reset(FakeDatabase *fdb) {
    fake_db_init(fdb);
}

void fake_db_install(FakeDatabase *fdb) {
    current_fdb = fdb;
}

// --- Implementacion fake: logica real pero simplificada ---

int db_find_user_by_email(Database *db, const char *email, UserRecord *out) {
    (void)db;  // Usamos current_fdb en su lugar
    if (!current_fdb || !email) return -1;

    for (int i = 0; i < current_fdb->count; i++) {
        if (strcmp(current_fdb->records[i].email, email) == 0) {
            if (out) {
                *out = current_fdb->records[i];
            }
            return 0;  // encontrado
        }
    }
    return -1;  // no encontrado
}

int db_insert_user(Database *db, const UserRecord *user) {
    (void)db;
    if (!current_fdb || !user) return -1;
    if (current_fdb->count >= FAKE_DB_MAX_RECORDS) return -1;

    // Verificar duplicado (como haria una DB real con UNIQUE constraint)
    for (int i = 0; i < current_fdb->count; i++) {
        if (strcmp(current_fdb->records[i].email, user->email) == 0) {
            return -1;  // duplicado
        }
    }

    // Insertar
    UserRecord *new_record = &current_fdb->records[current_fdb->count];
    *new_record = *user;
    new_record->id = current_fdb->next_id++;
    current_fdb->count++;

    return new_record->id;
}
```

```c
// test_user_service_with_fake.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "user_service.h"
#include "fake_database.h"

static FakeDatabase fdb;

static void setup(void) {
    fake_db_init(&fdb);
    fake_db_install(&fdb);
}

static void test_create_and_find(void) {
    setup();

    // Crear usuario — el fake inserta en el array interno
    int id = user_service_create("Alice", "alice@test.com", NULL);
    assert(id > 0);

    // Buscar — el fake busca en el array interno
    // Esto funciona porque el fake tiene ESTADO, a diferencia de un stub
    UserRecord found;
    int rc = db_find_user_by_email(NULL, "alice@test.com", &found);
    assert(rc == 0);
    assert(strcmp(found.name, "Alice") == 0);
    assert(found.id == id);

    printf("  PASS: create and find\n");
}

static void test_create_duplicate_rejected(void) {
    setup();

    int id1 = user_service_create("Alice", "alice@test.com", NULL);
    assert(id1 > 0);

    // Segundo create con mismo email — el fake detecta el duplicado
    int id2 = user_service_create("Alice2", "alice@test.com", NULL);
    assert(id2 == -1);  // rechazado

    printf("  PASS: duplicate rejected by fake DB\n");
}

static void test_create_multiple_users(void) {
    setup();

    int id1 = user_service_create("Alice", "alice@test.com", NULL);
    int id2 = user_service_create("Bob", "bob@test.com", NULL);
    int id3 = user_service_create("Charlie", "charlie@test.com", NULL);

    assert(id1 > 0 && id2 > 0 && id3 > 0);
    assert(id1 != id2 && id2 != id3);  // IDs unicos
    assert(fdb.count == 3);             // Podemos inspeccionar el estado interno

    printf("  PASS: create multiple users\n");
}

int main(void) {
    printf("=== user_service tests (con fake DB) ===\n");
    test_create_and_find();
    test_create_duplicate_rejected();
    test_create_multiple_users();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

### 6.3 Otros fakes comunes

```c
// --- Fake filesystem: in-memory ---
typedef struct {
    char path[256];
    char content[4096];
    int size;
} FakeFile;

typedef struct {
    FakeFile files[64];
    int count;
} FakeFS;

int fake_fs_write(FakeFS *fs, const char *path, const char *data, int len);
int fake_fs_read(FakeFS *fs, const char *path, char *buf, int buf_size);
int fake_fs_exists(FakeFS *fs, const char *path);

// --- Fake HTTP client: respuestas por URL ---
typedef struct {
    char url[256];
    int status_code;
    char body[4096];
} FakeHTTPResponse;

typedef struct {
    FakeHTTPResponse responses[32];
    int count;
} FakeHTTPClient;

void fake_http_add_response(FakeHTTPClient *c, const char *url,
                            int status, const char *body);
int fake_http_get(FakeHTTPClient *c, const char *url,
                  char *body, int body_size);

// --- Fake clock: tiempo controlado ---
typedef struct {
    time_t current_time;
} FakeClock;

void fake_clock_set(FakeClock *c, time_t t);
void fake_clock_advance(FakeClock *c, int seconds);
time_t fake_clock_now(FakeClock *c);
```

### 6.4 Fake vs stub — cuando usar cual

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  FAKE vs STUB — DECISION                                                        │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Usar STUB cuando:                                                              │
│  • Solo necesitas controlar UN valor de retorno                                 │
│  • El test no depende del estado entre llamadas                                 │
│  • El test es sobre como el codigo reacciona a UN resultado especifico          │
│  • Quieres tests ultra-simples y rapidos de escribir                            │
│                                                                                  │
│  Usar FAKE cuando:                                                              │
│  • El test depende de que las operaciones sean consistentes                     │
│    (insert + find deben funcionar juntos)                                       │
│  • Necesitas estado entre multiples llamadas                                    │
│  • Quieres testear flujos mas complejos (CRUD completo)                        │
│  • Quieres reutilizar el double en muchos tests                                │
│  • La dependencia es un storage (DB, filesystem, cache)                        │
│                                                                                  │
│  Trade-off:                                                                     │
│  • Stub: facil de escribir, fragil (cambiar la API = cambiar cada test)        │
│  • Fake: mas trabajo inicial, pero mas reutilizable y realista                  │
│  • Un fake mal implementado puede tener sus propios bugs                        │
│    (estas testeando tu fake, no tu codigo!)                                     │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Spy — grabar interacciones para verificar despues

### 7.1 Definicion

Un **spy** es como un stub pero ademas **graba** todas las llamadas que recibe: que funciones se llamaron, con que argumentos, cuantas veces, y en que orden. La verificacion se hace **despues** de ejecutar el codigo bajo test.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  SPY                                                                             │
│                                                                                  │
│  Pregunta que responde:  "¿Mi codigo llamo a la dependencia correctamente?"     │
│  Como lo verifica:       Grabando todo, y verificando despues (post-hoc)        │
│                                                                                  │
│  Ejemplo mental:                                                                │
│  ┌──────────────────────────────────────────────────────────┐                    │
│  │  test: "Cuando creo un usuario, ¿se envio un email     │                    │
│  │         de bienvenida a la direccion correcta?"          │                    │
│  │                                                          │                    │
│  │  Spy graba: send_email("alice@test.com", "Welcome!")    │                    │
│  │  Test verifica: spy.calls[0].to == "alice@test.com"     │                    │
│  │                 spy.calls[0].subject == "Welcome!"      │                    │
│  │                 spy.call_count == 1                      │                    │
│  └──────────────────────────────────────────────────────────┘                    │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 7.2 Ejemplo en C — spy de email

```c
// spy_email.h
#ifndef SPY_EMAIL_H
#define SPY_EMAIL_H

#define SPY_MAX_CALLS 32
#define SPY_MAX_STRING 256

typedef struct {
    char to[SPY_MAX_STRING];
    char subject[SPY_MAX_STRING];
    char body[1024];
} EmailCall;

typedef struct {
    EmailCall calls[SPY_MAX_CALLS];
    int call_count;
    int return_value;  // que retorna el spy (tambien es un stub)
} EmailSpy;

void email_spy_init(EmailSpy *spy);
void email_spy_set_return(EmailSpy *spy, int value);
void email_spy_install(EmailSpy *spy);

// Funciones de consulta para los tests
int email_spy_call_count(const EmailSpy *spy);
const EmailCall *email_spy_get_call(const EmailSpy *spy, int index);
int email_spy_was_called_with(const EmailSpy *spy, const char *to,
                               const char *subject);

#endif
```

```c
// spy_email.c
#include "spy_email.h"
#include "email.h"  // interfaz real: int send_email(const char *to, ...)
#include <string.h>

static EmailSpy *current_spy = NULL;

void email_spy_init(EmailSpy *spy) {
    memset(spy, 0, sizeof(*spy));
    spy->return_value = 0;  // default: exito
}

void email_spy_set_return(EmailSpy *spy, int value) {
    spy->return_value = value;
}

void email_spy_install(EmailSpy *spy) {
    current_spy = spy;
}

int email_spy_call_count(const EmailSpy *spy) {
    return spy->call_count;
}

const EmailCall *email_spy_get_call(const EmailSpy *spy, int index) {
    if (index < 0 || index >= spy->call_count) return NULL;
    return &spy->calls[index];
}

int email_spy_was_called_with(const EmailSpy *spy, const char *to,
                               const char *subject) {
    for (int i = 0; i < spy->call_count; i++) {
        if (strcmp(spy->calls[i].to, to) == 0 &&
            strcmp(spy->calls[i].subject, subject) == 0) {
            return 1;
        }
    }
    return 0;
}

// --- Implementacion espia ---
// Reemplaza la funcion real send_email
int send_email(const char *to, const char *subject, const char *body) {
    if (!current_spy) return -1;
    if (current_spy->call_count >= SPY_MAX_CALLS) return -1;

    // GRABAR la llamada
    EmailCall *call = &current_spy->calls[current_spy->call_count];
    snprintf(call->to, SPY_MAX_STRING, "%s", to ? to : "");
    snprintf(call->subject, SPY_MAX_STRING, "%s", subject ? subject : "");
    snprintf(call->body, sizeof(call->body), "%s", body ? body : "");
    current_spy->call_count++;

    // Retornar el valor pre-configurado (comportamiento de stub)
    return current_spy->return_value;
}
```

```c
// test_user_service_spy.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "user_service.h"
#include "spy_email.h"
#include "fake_database.h"

static FakeDatabase fdb;
static EmailSpy email_spy;

static void setup(void) {
    fake_db_init(&fdb);
    fake_db_install(&fdb);
    email_spy_init(&email_spy);
    email_spy_install(&email_spy);
}

static void test_create_user_sends_welcome_email(void) {
    setup();

    user_service_create("Alice", "alice@test.com", NULL);

    // Verificar que se envio exactamente UN email
    assert(email_spy_call_count(&email_spy) == 1);

    // Verificar los argumentos del email
    const EmailCall *call = email_spy_get_call(&email_spy, 0);
    assert(call != NULL);
    assert(strcmp(call->to, "alice@test.com") == 0);
    assert(strcmp(call->subject, "Welcome!") == 0);

    printf("  PASS: welcome email sent to correct address\n");
}

static void test_create_user_duplicate_does_not_send_email(void) {
    setup();

    // Crear primer usuario
    user_service_create("Alice", "alice@test.com", NULL);

    // Resetear spy para contar solo las llamadas del segundo intento
    email_spy_init(&email_spy);
    email_spy_install(&email_spy);

    // Intentar crear duplicado
    user_service_create("Alice2", "alice@test.com", NULL);

    // No debe enviar email para un usuario duplicado
    assert(email_spy_call_count(&email_spy) == 0);

    printf("  PASS: no email sent for duplicate user\n");
}

static void test_create_user_email_failure_does_not_block_creation(void) {
    setup();
    // Configurar spy para que send_email "falle"
    email_spy_set_return(&email_spy, -1);

    // create_user deberia funcionar incluso si el email falla
    int user_id = user_service_create("Bob", "bob@test.com", NULL);
    assert(user_id > 0);  // usuario creado a pesar del error de email

    // Verificar que SI intento enviar el email (aunque fallo)
    assert(email_spy_call_count(&email_spy) == 1);

    printf("  PASS: user created even when email fails\n");
}

int main(void) {
    printf("=== user_service tests (con spy de email) ===\n");
    test_create_user_sends_welcome_email();
    test_create_user_duplicate_does_not_send_email();
    test_create_user_email_failure_does_not_block_creation();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

### 7.3 Spy vs stub

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  STUB vs SPY                                                                    │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Un spy ES un stub (retorna valores configurados) + grabacion.                  │
│  Todo spy es un stub. No todo stub es un spy.                                   │
│                                                                                  │
│  Stub:                              Spy:                                        │
│  - Controla INPUT al codigo         - Controla INPUT al codigo                  │
│  - No verifica nada                 - Verifica OUTPUT del codigo                │
│  - No graba nada                    - Graba llamadas                            │
│                                                                                  │
│  ¿Cuando usar spy en vez de stub?                                               │
│  Cuando tu test necesita verificar que el codigo LLAMO a la dependencia         │
│  correctamente (no solo como reacciono al resultado).                           │
│                                                                                  │
│  Ejemplo:                                                                       │
│  - "¿create_user hashea la password antes de guardarla?"                        │
│    → SPY: grabar el argumento de db_insert y verificar que la password          │
│           esta hasheada (no en texto plano)                                      │
│  - "¿create_user retorna -1 cuando db_insert falla?"                            │
│    → STUB: configurar db_insert para retornar -1, verificar que                 │
│           create_user retorna -1                                                │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Mock — expectativas verificadas automaticamente

### 8.1 Definicion

Un **mock** tiene **expectativas pre-definidas** sobre como debe ser llamado. A diferencia del spy (que graba y verifica despues), el mock **verifica durante la ejecucion** o al final de forma automatica. Si el codigo no llama al mock como se espera, el test falla.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  MOCK                                                                            │
│                                                                                  │
│  Pregunta que responde:  "¿Mi codigo interactuo con la dependencia              │
│                           EXACTAMENTE como yo esperaba?"                         │
│                                                                                  │
│  Como lo verifica:       Expectativas PRE-DEFINIDAS, verificacion automatica    │
│                                                                                  │
│  SPY (post-hoc):                                                                │
│  1. Ejecutar codigo                                                             │
│  2. Inspeccionar grabaciones                                                    │
│  3. Verificar manualmente en asserts                                            │
│                                                                                  │
│  MOCK (pre-definido):                                                           │
│  1. Definir expectativas: "espero que llamen a X con argumentos Y"              │
│  2. Ejecutar codigo                                                             │
│  3. El framework verifica automaticamente                                       │
│     (si no se cumplen las expectativas, falla sin assert explicito)             │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Ejemplo en C con CMocka (el mock mas natural en C)

```c
// CMocka proporciona mock() para definir valores de retorno
// y expect_*() para definir expectativas sobre argumentos

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

// Declarar la funcion que vamos a mockear
int send_email(const char *to, const char *subject, const char *body);

// Mock implementation (linked via --wrap)
int __wrap_send_email(const char *to, const char *subject, const char *body) {
    // EXPECTATIVAS: verificar que fue llamada con los argumentos correctos
    check_expected(to);
    check_expected(subject);
    // body no lo verificamos (no nos importa en este test)
    (void)body;

    // Retornar el valor pre-configurado
    return mock_type(int);
}

// Test
static void test_create_user_sends_correct_email(void **state) {
    (void)state;

    // DEFINIR EXPECTATIVAS (antes de ejecutar el codigo)
    expect_string(__wrap_send_email, to, "alice@test.com");
    expect_string(__wrap_send_email, subject, "Welcome!");
    will_return(__wrap_send_email, 0);  // simular exito

    // EJECUTAR el codigo bajo test
    int result = user_service_create("Alice", "alice@test.com", NULL);

    // Si send_email NO fue llamado con to="alice@test.com" y subject="Welcome!",
    // CMocka ya habria fallado el test AUTOMATICAMENTE.
    // No necesitamos assert explicitos para la interaccion.
    assert_int_equal(result, 1);  // solo verificar el resultado
}

// Si send_email NO se llama, CMocka falla:
// "Expected function called but was not: send_email"
//
// Si send_email se llama con argumentos incorrectos, CMocka falla:
// "Expected string for parameter 'to': 'alice@test.com', got 'wrong@email.com'"
```

### 8.3 Mock manual en C (sin CMocka)

```c
// Es posible implementar mocks sin CMocka, pero es mas verbose

typedef struct {
    // Expectativas
    const char *expected_to;
    const char *expected_subject;
    int expected_call_count;

    // Estado
    int actual_call_count;
    int return_value;
    int expectations_met;
} EmailMock;

static EmailMock email_mock;

void email_mock_init(EmailMock *m) {
    memset(m, 0, sizeof(*m));
    m->expectations_met = 1;  // true hasta que falle
}

void email_mock_expect_call(EmailMock *m, const char *to,
                            const char *subject, int return_val) {
    m->expected_to = to;
    m->expected_subject = subject;
    m->expected_call_count = 1;
    m->return_value = return_val;
}

int email_mock_verify(const EmailMock *m) {
    if (m->actual_call_count != m->expected_call_count) {
        fprintf(stderr, "MOCK FAIL: expected %d calls, got %d\n",
                m->expected_call_count, m->actual_call_count);
        return 0;
    }
    return m->expectations_met;
}

// Implementacion mock
int send_email(const char *to, const char *subject, const char *body) {
    (void)body;
    email_mock.actual_call_count++;

    // Verificar expectativas DURANTE la llamada
    if (email_mock.expected_to && strcmp(to, email_mock.expected_to) != 0) {
        fprintf(stderr, "MOCK FAIL: expected to='%s', got '%s'\n",
                email_mock.expected_to, to);
        email_mock.expectations_met = 0;
    }
    if (email_mock.expected_subject &&
        strcmp(subject, email_mock.expected_subject) != 0) {
        fprintf(stderr, "MOCK FAIL: expected subject='%s', got '%s'\n",
                email_mock.expected_subject, subject);
        email_mock.expectations_met = 0;
    }

    return email_mock.return_value;
}

// Test
static void test_with_manual_mock(void) {
    email_mock_init(&email_mock);

    // Definir expectativas
    email_mock_expect_call(&email_mock, "alice@test.com", "Welcome!", 0);

    // Ejecutar
    user_service_create("Alice", "alice@test.com", NULL);

    // Verificar que las expectativas se cumplieron
    assert(email_mock_verify(&email_mock));
    printf("  PASS: mock expectations met\n");
}
```

### 8.4 Caracteristicas del mock

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  MOCK — PROPIEDADES                                                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ✓ Verifica argumentos de llamada (como spy, pero automatico)                   │
│  ✓ Verifica numero de llamadas                                                  │
│  ✓ Verifica orden de llamadas (opcional, depende del framework)                 │
│  ✓ Retorna valores configurados (como stub)                                     │
│  ✓ Falla automaticamente si las expectativas no se cumplen                      │
│  ✓ Mensajes de error descriptivos (que se esperaba vs que se recibio)           │
│                                                                                  │
│  ✗ Tests mas fragiles: si cambias la implementacion interna                     │
│    (sin cambiar el comportamiento), los mocks fallan                            │
│  ✗ Tests mas complejos de escribir y mantener                                   │
│  ✗ Pueden llevar a sobre-especificar (mockear demasiado)                        │
│                                                                                  │
│  Uso principal: verificar INTERACCIONES del codigo con sus dependencias         │
│  (no solo el resultado, sino COMO llego al resultado)                           │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Spy vs Mock — la diferencia sutil

Esta es la distincion que mas confunde. Ambos verifican interacciones. La diferencia es **cuando y como**:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  SPY vs MOCK — LA DIFERENCIA                                                    │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  SPY (Tell Don't Ask → Ask):                                                    │
│  ┌────────────────────────────────────────────────┐                              │
│  │  1. spy = create_spy()                          │                              │
│  │  2. ejecutar codigo bajo test                   │  ← spy graba silenciosamente│
│  │  3. assert(spy.calls[0].to == "alice@test.com") │  ← verificas TU, con assert│
│  │  4. assert(spy.call_count == 1)                 │  ← verificas TU, con assert│
│  └────────────────────────────────────────────────┘                              │
│                                                                                  │
│  MOCK (Hollywood Principle: "don't call us, we'll call you"):                   │
│  ┌────────────────────────────────────────────────┐                              │
│  │  1. expect_string(mock, to, "alice@test.com")   │  ← defines expectativa     │
│  │  2. expect_call_count(mock, 1)                  │  ← defines expectativa     │
│  │  3. ejecutar codigo bajo test                   │  ← mock verifica EN VIVO   │
│  │  4. (el framework verifica automaticamente)     │  ← no necesitas asserts    │
│  └────────────────────────────────────────────────┘                              │
│                                                                                  │
│  En la practica, la linea es borrosa:                                           │
│  • CMocka: expect_*() + check_expected() = mock clasico                         │
│  • CMocka: si no usas expect_*, y solo grabas, es un spy                        │
│  • Un spy con verificacion automatica al final ES un mock                       │
│  • Muchos frameworks mezclan ambos conceptos                                    │
│                                                                                  │
│  Regla practica:                                                                │
│  Si verificas con assert() manual despues → SPY                                 │
│  Si el framework verifica automaticamente → MOCK                                │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 9.1 Ejemplo lado a lado

```c
// ===== MISMO TEST, ESTILO SPY =====
static void test_welcome_email_spy_style(void) {
    // Setup
    EmailSpy spy;
    email_spy_init(&spy);
    email_spy_install(&spy);

    // Act
    user_service_create("Alice", "alice@test.com", NULL);

    // Assert (manual, post-hoc)
    assert(email_spy_call_count(&spy) == 1);
    const EmailCall *call = email_spy_get_call(&spy, 0);
    assert(strcmp(call->to, "alice@test.com") == 0);
    assert(strcmp(call->subject, "Welcome!") == 0);
}

// ===== MISMO TEST, ESTILO MOCK (CMocka) =====
static void test_welcome_email_mock_style(void **state) {
    (void)state;

    // Arrange: definir expectativas ANTES
    expect_string(__wrap_send_email, to, "alice@test.com");
    expect_string(__wrap_send_email, subject, "Welcome!");
    will_return(__wrap_send_email, 0);

    // Act
    user_service_create("Alice", "alice@test.com", NULL);

    // Assert: no necesitas assert para la interaccion
    // CMocka ya verifico que send_email fue llamado correctamente
    // (si no, el test habria fallado automaticamente)
}
```

---

## 10. Tabla resumen — los cinco tipos

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    RESUMEN DE LOS 5 TIPOS                                       │
├──────────┬──────────────────────┬──────────────────┬───────────────────────────  │
│ Tipo     │ Que hace             │ Que verifica     │ Ejemplo en C               │
├──────────┼──────────────────────┼──────────────────┼───────────────────────────  │
│ Dummy    │ Existe, nada mas     │ Nada             │ NULL, struct vacio          │
│ Stub     │ Retorna valor fijo   │ Nada             │ return 42; (ignora args)    │
│ Fake     │ Implementacion       │ Nada (indirecto) │ DB en array, FS en memoria  │
│          │ simplificada         │                  │                              │
│ Spy      │ Graba interacciones  │ Post-hoc con     │ Struct con calls[], verificar│
│          │ + retorna valor      │ assert manual    │ despues con assert           │
│ Mock     │ Pre-define           │ Automatico       │ CMocka expect_*() +          │
│          │ expectativas         │ (framework)      │ check_expected()             │
├──────────┼──────────────────────┼──────────────────┼───────────────────────────  │
│          │ CONTROLA INPUT       │ VERIFICA OUTPUT  │ COMPLEJIDAD                  │
│ Dummy    │ No                   │ No               │ Trivial                      │
│ Stub     │ Si                   │ No               │ Baja                         │
│ Fake     │ Si (con logica)      │ No               │ Media-Alta                   │
│ Spy      │ Si                   │ Si (manual)      │ Media                        │
│ Mock     │ Si                   │ Si (automatico)  │ Media-Alta                   │
└──────────┴──────────────────────┴──────────────────┴───────────────────────────  ┘
```

---

## 11. Mecanismos de inyeccion en C

Para reemplazar una dependencia real con un test double, necesitas un **mecanismo de inyeccion**: una forma de decirle al codigo "usa ESTO en vez de la dependencia real". En C hay cuatro mecanismos principales:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│              MECANISMOS DE INYECCION EN C                                       │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  1. LINK-TIME SUBSTITUTION                                                      │
│     Compilar con mock.o en lugar de real.o                                      │
│     gcc test.c user_service.c stub_database.c   (en vez de database.c)          │
│     → El linker resuelve db_find → stub, no → real                             │
│     → Cubierto en detalle en T03 de esta seccion                               │
│                                                                                  │
│  2. FUNCTION POINTERS (Dependency Injection natural de C)                       │
│     Pasar las dependencias como function pointers en un struct                  │
│     → El codigo llama deps->find() en vez de db_find()                         │
│     → En tests, deps apunta a funciones fake/stub/spy                          │
│     → Cubierto en detalle en T02 de esta seccion                               │
│                                                                                  │
│  3. LINKER WRAPPING (--wrap)                                                    │
│     gcc -Wl,--wrap=send_email test.c user_service.c                            │
│     → Cada llamada a send_email se redirige a __wrap_send_email                │
│     → La original queda accesible como __real_send_email                        │
│     → Cubierto en detalle en T04 de esta seccion (CMocka mocking)              │
│                                                                                  │
│  4. PREPROCESSOR MACROS                                                         │
│     #ifdef TESTING                                                              │
│     #define send_email mock_send_email                                          │
│     #endif                                                                      │
│     → Sucio pero funcional. No requiere cambios en el linker.                   │
│     → Problemas: namespace pollution, dificil de mantener                       │
│                                                                                  │
│  5. WEAK SYMBOLS (__attribute__((weak)))                                        │
│     La implementacion real se marca como weak.                                  │
│     El test provee una implementacion strong que la reemplaza.                  │
│     → Cubierto en T03 (Link-time substitution)                                 │
│                                                                                  │
│  Comparacion:                                                                   │
│  ┌──────────────────┬───────────┬───────────┬──────────┬────────────┐           │
│  │ Mecanismo        │ Invasivo  │ Flexible  │ Portable │ Frameworks │           │
│  ├──────────────────┼───────────┼───────────┼──────────┼────────────┤           │
│  │ Link-time subst  │ No        │ Por file  │ Si       │ Todos      │           │
│  │ Function pointer │ Si (API)  │ Per-call  │ Si       │ Todos      │           │
│  │ --wrap           │ No        │ Per-func  │ GNU ld   │ CMocka     │           │
│  │ Preprocessor     │ Si (src)  │ Limited   │ Si       │ Todos      │           │
│  │ Weak symbols     │ Si (src)  │ Per-func  │ GCC/Clng │ Todos      │           │
│  └──────────────────┴───────────┴───────────┴──────────┴────────────┘           │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 11.1 Preview de cada mecanismo

Los tres topicos siguientes de esta seccion cubren los mecanismos en profundidad. Aqui un preview rapido de cada uno:

```c
// === 1. LINK-TIME SUBSTITUTION ===
// real_database.c: implementa db_find() con PostgreSQL
// stub_database.c: implementa db_find() con return -1
// En test: gcc test.c user_service.c stub_database.c  ← usa stub
// En prod: gcc main.c user_service.c real_database.c  ← usa real


// === 2. FUNCTION POINTERS ===
typedef struct {
    int (*find_user)(const char *email, UserRecord *out);
    int (*insert_user)(const UserRecord *user);
} DatabaseOps;

int user_service_create(const char *name, const char *email,
                        const DatabaseOps *db) {
    UserRecord rec;
    if (db->find_user(email, &rec) == 0) return -1;  // duplicado
    // ...
    return db->insert_user(&new_user);
}

// En test:
static int stub_find(const char *email, UserRecord *out) { return -1; }
static int stub_insert(const UserRecord *u) { return 42; }
DatabaseOps stub_db = { .find_user = stub_find, .insert_user = stub_insert };
user_service_create("Alice", "alice@test.com", &stub_db);


// === 3. LINKER WRAPPING (--wrap) ===
// gcc -Wl,--wrap=db_find_user_by_email test.c user_service.c database.c
// Ahora cada llamada a db_find_user_by_email va a __wrap_db_find_user_by_email
int __wrap_db_find_user_by_email(Database *db, const char *email,
                                  UserRecord *out) {
    // mock/stub/spy implementation
    return mock_type(int);  // CMocka
}


// === 4. PREPROCESSOR ===
#ifdef TESTING
#define db_find_user_by_email stub_db_find_user_by_email
#endif
// Cuando compilas con -DTESTING, todas las llamadas a db_find_user_by_email
// se reemplazan por stub_db_find_user_by_email en el preprocesador


// === 5. WEAK SYMBOLS ===
// database.c:
__attribute__((weak))
int db_find_user_by_email(Database *db, const char *email, UserRecord *out) {
    // implementacion real (PostgreSQL)
}

// test_database.c:
// Implementacion "strong" que reemplaza la weak
int db_find_user_by_email(Database *db, const char *email, UserRecord *out) {
    return -1;  // stub
}
```

---

## 12. Anti-patrones — errores comunes con test doubles

### 12.1 Mockear todo

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  ANTI-PATRON: MOCKEAR TODO                                                      │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  MALO:                                                                          │
│  static void test_add(void) {                                                   │
│      // Mockear la suma... ¿por que?                                            │
│      expect_value(mock_add, a, 2);                                              │
│      expect_value(mock_add, b, 3);                                              │
│      will_return(mock_add, 5);                                                  │
│      assert_int_equal(calculator_add(2, 3), 5);                                │
│  }                                                                              │
│  // Este test no testea NADA. Solo verifica que el mock funciona.              │
│  // No hay logica real ejecutandose.                                            │
│                                                                                  │
│  REGLA: Solo mockear BOUNDARIES (fronteras del sistema)                         │
│  • Si: DB, red, filesystem, reloj, hardware, servicios externos                │
│  • No: funciones puras, logica interna, utilidades, helpers                     │
│  • No: funciones del mismo modulo                                               │
│                                                                                  │
│  Si todo esta mockeado, no estas testeando tu codigo.                           │
│  Estas testeando tu capacidad de configurar mocks.                              │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 12.2 Tests fragiles por sobre-especificacion

```c
// MALO: verificar detalles de implementacion irrelevantes
static void test_create_user_fragile(void **state) {
    // Expectativa fragil: verifica el query SQL exacto
    expect_string(__wrap_db_query, sql,
        "INSERT INTO users (name, email) VALUES ('Alice', 'alice@test.com')");
    will_return(__wrap_db_query, 0);

    user_service_create("Alice", "alice@test.com", NULL);
}
// Si alguien cambia el query a usar prepared statements,
// o cambia el orden de los campos, o agrega un campo,
// este test se rompe aunque el comportamiento es correcto.

// MEJOR: verificar solo lo relevante
static void test_create_user_robust(void **state) {
    // Solo verificar que se inserto un usuario con el email correcto
    // No importa el SQL exacto
    expect_any(__wrap_db_insert_user, user);  // no verificar contenido exacto
    will_return(__wrap_db_insert_user, 42);

    int id = user_service_create("Alice", "alice@test.com", NULL);
    assert_int_equal(id, 42);
}
```

### 12.3 Mock que diverge de la realidad

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  ANTI-PATRON: EL MOCK MENTIROSO                                                │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Tu mock de db_find_user_by_email retorna un UserRecord.                        │
│  La DB real retorna un error porque el schema cambio.                           │
│  El mock sigue retornando datos del schema viejo.                               │
│                                                                                  │
│  → Los tests pasan. La produccion falla.                                        │
│                                                                                  │
│  Solucion:                                                                      │
│  • Tener TAMBIEN tests de integracion contra la DB real                         │
│  • Actualizar los mocks cuando cambia la interfaz                               │
│  • Usar contratos/interfaces explicitas que mock Y real implementan             │
│  • Los test doubles solo evitan la NECESIDAD de la dependencia,                 │
│    no la OBLIGACION de testear contra ella eventualmente                        │
│                                                                                  │
│  Piramide de testing:                                                           │
│  ┌─────┐  E2E (pocos, lentos, contra todo real)                                │
│  ┌─────────┐  Integracion (medios, contra DB/API real)                          │
│  ┌─────────────────┐  Unitarios con doubles (muchos, rapidos)                   │
│                                                                                  │
│  Los doubles NO reemplazan los tests de integracion.                            │
│  Los COMPLEMENTAN para el desarrollo rapido dia a dia.                          │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 12.4 Estado global del mock compartido entre tests

```c
// MALO: mock con estado global que no se resetea
static int mock_counter = 0;  // compartido entre tests

int mock_db_insert(const UserRecord *u) {
    mock_counter++;
    return mock_counter;  // ID = numero de llamadas acumuladas
}

// test_A: mock_counter vale 1 despues
// test_B: mock_counter vale 2 → resultado inesperado
// test_C: depende del orden de ejecucion

// BIEN: resetear en cada test
static void setup(void) {
    mock_counter = 0;  // reset
}
// O mejor: cada test crea su propia instancia del mock
```

---

## 13. Reglas practicas para elegir el tipo de double

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                   GUIA DE DECISION                                               │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ¿Necesitas la dependencia para que compile?                                    │
│  └── Si, pero no la uso → DUMMY (NULL o struct vacio)                          │
│                                                                                  │
│  ¿Necesitas controlar que retorna la dependencia?                               │
│  └── Si, un valor fijo → STUB                                                  │
│  └── Si, con logica (insert→find funciona) → FAKE                             │
│                                                                                  │
│  ¿Necesitas verificar como se llamo a la dependencia?                           │
│  └── Si, con asserts manuales → SPY                                            │
│  └── Si, con verificacion automatica → MOCK                                    │
│                                                                                  │
│  ¿Que tipo es MAS COMUN en C?                                                  │
│  1. STUB (link-time substitution) — con diferencia el mas usado                │
│  2. FAKE (DB en memoria, FS en memoria) — para storage                         │
│  3. MOCK (CMocka expect/will_return) — para verificar interacciones            │
│  4. SPY (structs con call recording) — manual en C                              │
│  5. DUMMY (NULL) — trivial                                                      │
│                                                                                  │
│  ¿Que tipo es MEJOR para empezar?                                              │
│  → STUB. Es el mas simple y cubre la mayoria de casos.                         │
│  → Si necesitas mas, agrega spy o fake segun el caso.                          │
│  → Usa mocks (CMocka) cuando la verificacion de interaccion es critica.        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 14. Comparacion con Rust y Go

### 14.1 Rust — traits como interfaz para mocking

```rust
// Rust usa traits (interfaces) para inyectar dependencias.
// mockall genera implementaciones mock automaticamente.

// Trait = interfaz
trait Database {
    fn find_user_by_email(&self, email: &str) -> Option<User>;
    fn insert_user(&self, user: &User) -> Result<i32, DbError>;
}

// Implementacion real
struct PostgresDB { /* ... */ }
impl Database for PostgresDB {
    fn find_user_by_email(&self, email: &str) -> Option<User> {
        // query real a PostgreSQL
    }
    fn insert_user(&self, user: &User) -> Result<i32, DbError> {
        // insert real
    }
}

// Mock generado automaticamente por mockall
#[cfg(test)]
use mockall::automock;

#[automock]
trait Database {
    fn find_user_by_email(&self, email: &str) -> Option<User>;
    fn insert_user(&self, user: &User) -> Result<i32, DbError>;
}

#[test]
fn test_create_user() {
    let mut mock_db = MockDatabase::new();

    // Expectativas (estilo mock)
    mock_db.expect_find_user_by_email()
        .with(eq("alice@test.com"))
        .returning(|_| None);  // usuario no existe

    mock_db.expect_insert_user()
        .returning(|_| Ok(42));

    let service = UserService::new(Box::new(mock_db));
    let result = service.create_user("Alice", "alice@test.com");
    assert_eq!(result, Ok(42));
    // mockall verifica automaticamente que las expectativas se cumplieron
}
```

**Diferencia clave**: En Rust, el compilador verifica que el mock implementa el trait completo. En C, si tu mock olvida una funcion, el linker puede o no reportar error (depende de si se llama).

### 14.2 Go — interfaces implicitas

```go
// Go usa interfaces implicitas: cualquier struct que tenga los metodos
// de la interfaz la implementa automaticamente. No necesita "implements".

// Interfaz
type Database interface {
    FindUserByEmail(email string) (*User, error)
    InsertUser(user *User) (int, error)
}

// Implementacion real
type PostgresDB struct { /* ... */ }
func (db *PostgresDB) FindUserByEmail(email string) (*User, error) { /* ... */ }
func (db *PostgresDB) InsertUser(user *User) (int, error) { /* ... */ }

// Fake para tests
type FakeDB struct {
    Users map[string]*User
    NextID int
}

func (db *FakeDB) FindUserByEmail(email string) (*User, error) {
    user, ok := db.Users[email]
    if !ok {
        return nil, errors.New("not found")
    }
    return user, nil
}

func (db *FakeDB) InsertUser(user *User) (int, error) {
    db.NextID++
    user.ID = db.NextID
    db.Users[user.Email] = user
    return user.ID, nil
}

func TestCreateUser(t *testing.T) {
    fakeDB := &FakeDB{Users: make(map[string]*User), NextID: 0}
    service := NewUserService(fakeDB)

    id, err := service.CreateUser("Alice", "alice@test.com")
    if err != nil {
        t.Fatal(err)
    }
    if id != 1 {
        t.Errorf("expected id=1, got %d", id)
    }
}
```

**Diferencia clave**: Go usa interfaces + fakes/stubs manuales por defecto. Los mocks automaticos (gomock, testify/mock) existen pero la comunidad Go prefiere fakes simples. Es la filosofia opuesta a CMocka (que prioriza mocks).

### 14.3 Tabla comparativa de mecanismos

| Aspecto | C | Rust | Go |
|---------|---|------|----|
| **Interfaz para DI** | Function pointers, headers | Traits (explicito) | Interfaces (implicito) |
| **Inyeccion** | Link-time, function pointers, --wrap | Generic/trait bounds, Box\<dyn Trait\> | Interface parameter |
| **Mock automatico** | No (manual o CMocka) | mockall crate | gomock (generate) |
| **Fake comun** | Struct + funciones | Implementacion de trait | Implementacion de interface |
| **Spy** | Manual (struct con calls[]) | mockall (built-in) | Manual o testify/mock |
| **Stub** | Link-time (archivo .o diferente) | mockall `.returning()` | Struct con valores fijos |
| **Verificar el compilador** | No (linker errors, maybe) | Si (trait bounds) | Si (interface compliance) |
| **Overhead en produccion** | Ninguno (link-time) o function pointer call | Ninguno (static dispatch) o vtable | Interface dispatch |

---

## 15. Ejemplo completo — servicio de notificaciones

Un ejemplo que usa los cinco tipos de test doubles en un solo proyecto.

### 15.1 Codigo de produccion

```c
// notification_service.h
#ifndef NOTIFICATION_SERVICE_H
#define NOTIFICATION_SERVICE_H

#include <stdbool.h>
#include <time.h>

// --- Dependencias (interfaces) ---

// Base de datos de usuarios
typedef struct {
    int id;
    char name[64];
    char email[128];
    char phone[20];
    bool email_enabled;
    bool sms_enabled;
} UserProfile;

int user_db_find(int user_id, UserProfile *out);

// Envio de email
int email_send(const char *to, const char *subject, const char *body);

// Envio de SMS
int sms_send(const char *phone, const char *message);

// Reloj
time_t clock_now(void);

// Logger
void logger_info(const char *msg);
void logger_error(const char *msg);

// --- Servicio ---
typedef enum {
    NOTIFY_OK = 0,
    NOTIFY_USER_NOT_FOUND,
    NOTIFY_NO_CHANNELS,
    NOTIFY_ALL_FAILED,
    NOTIFY_PARTIAL,
} NotifyResult;

NotifyResult notify_user(int user_id, const char *subject, const char *message);

// Verificar si estamos en horario de notificaciones (8am - 10pm)
bool is_notification_hour(time_t t);

#endif
```

```c
// notification_service.c
#include "notification_service.h"
#include <stdio.h>
#include <string.h>

bool is_notification_hour(time_t t) {
    struct tm *tm = localtime(&t);
    return tm->tm_hour >= 8 && tm->tm_hour < 22;
}

NotifyResult notify_user(int user_id, const char *subject, const char *message) {
    // 1. Buscar usuario
    UserProfile user;
    if (user_db_find(user_id, &user) != 0) {
        logger_error("User not found");
        return NOTIFY_USER_NOT_FOUND;
    }

    // 2. Verificar horario
    time_t now = clock_now();
    if (!is_notification_hour(now)) {
        logger_info("Outside notification hours, skipping");
        return NOTIFY_OK;  // no es error, simplemente no notificamos
    }

    // 3. Verificar que tiene al menos un canal habilitado
    if (!user.email_enabled && !user.sms_enabled) {
        logger_info("User has no notification channels enabled");
        return NOTIFY_NO_CHANNELS;
    }

    // 4. Enviar por cada canal habilitado
    int attempts = 0;
    int successes = 0;

    if (user.email_enabled) {
        attempts++;
        char body[512];
        snprintf(body, sizeof(body), "Hi %s,\n\n%s", user.name, message);
        if (email_send(user.email, subject, body) == 0) {
            successes++;
            logger_info("Email sent successfully");
        } else {
            logger_error("Email send failed");
        }
    }

    if (user.sms_enabled) {
        attempts++;
        char sms_text[160];
        snprintf(sms_text, sizeof(sms_text), "%s: %s", subject, message);
        if (sms_send(user.phone, sms_text) == 0) {
            successes++;
            logger_info("SMS sent successfully");
        } else {
            logger_error("SMS send failed");
        }
    }

    if (successes == 0) return NOTIFY_ALL_FAILED;
    if (successes < attempts) return NOTIFY_PARTIAL;
    return NOTIFY_OK;
}
```

### 15.2 Tests con los cinco tipos de doubles

```c
// test_notification_service.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "notification_service.h"

// =====================================================================
// 1. DUMMY — logger que no hace nada (no nos importa el logging)
// =====================================================================

void logger_info(const char *msg) {
    (void)msg;  // DUMMY: ignora completamente
}

void logger_error(const char *msg) {
    (void)msg;  // DUMMY: ignora completamente
}

// =====================================================================
// 2. STUB — clock que retorna un valor fijo
// =====================================================================

static time_t stub_time = 0;

time_t clock_now(void) {
    return stub_time;  // STUB: siempre retorna stub_time
}

static void set_stub_time_to_noon(void) {
    // 2024-01-15 12:00:00 (mediodia, dentro de horario)
    struct tm noon = {
        .tm_year = 124, .tm_mon = 0, .tm_mday = 15,
        .tm_hour = 12, .tm_min = 0, .tm_sec = 0, .tm_isdst = -1
    };
    stub_time = mktime(&noon);
}

static void set_stub_time_to_midnight(void) {
    // 2024-01-15 02:00:00 (madrugada, fuera de horario)
    struct tm midnight = {
        .tm_year = 124, .tm_mon = 0, .tm_mday = 15,
        .tm_hour = 2, .tm_min = 0, .tm_sec = 0, .tm_isdst = -1
    };
    stub_time = mktime(&midnight);
}

// =====================================================================
// 3. FAKE — base de datos de usuarios en memoria
// =====================================================================

#define FAKE_MAX_USERS 10
static UserProfile fake_users[FAKE_MAX_USERS];
static int fake_user_count = 0;

void fake_user_db_reset(void) {
    fake_user_count = 0;
    memset(fake_users, 0, sizeof(fake_users));
}

void fake_user_db_add(int id, const char *name, const char *email,
                      const char *phone, bool email_en, bool sms_en) {
    UserProfile *u = &fake_users[fake_user_count++];
    u->id = id;
    snprintf(u->name, sizeof(u->name), "%s", name);
    snprintf(u->email, sizeof(u->email), "%s", email);
    snprintf(u->phone, sizeof(u->phone), "%s", phone);
    u->email_enabled = email_en;
    u->sms_enabled = sms_en;
}

int user_db_find(int user_id, UserProfile *out) {
    // FAKE: busqueda real en array (no retorno fijo)
    for (int i = 0; i < fake_user_count; i++) {
        if (fake_users[i].id == user_id) {
            if (out) *out = fake_users[i];
            return 0;
        }
    }
    return -1;
}

// =====================================================================
// 4. SPY — email_send que graba las llamadas
// =====================================================================

#define SPY_MAX_CALLS 16

typedef struct {
    char to[128];
    char subject[128];
    char body[512];
} EmailCallRecord;

static EmailCallRecord email_calls[SPY_MAX_CALLS];
static int email_call_count = 0;
static int email_return_value = 0;

void email_spy_reset(void) {
    email_call_count = 0;
    email_return_value = 0;
    memset(email_calls, 0, sizeof(email_calls));
}

int email_send(const char *to, const char *subject, const char *body) {
    // SPY: graba la llamada
    if (email_call_count < SPY_MAX_CALLS) {
        EmailCallRecord *call = &email_calls[email_call_count];
        snprintf(call->to, sizeof(call->to), "%s", to ? to : "");
        snprintf(call->subject, sizeof(call->subject), "%s", subject ? subject : "");
        snprintf(call->body, sizeof(call->body), "%s", body ? body : "");
    }
    email_call_count++;
    return email_return_value;  // tambien es stub
}

// =====================================================================
// 5. MOCK — sms_send con expectativas pre-definidas
// =====================================================================

static char sms_expected_phone[20] = "";
static char sms_expected_message_prefix[64] = "";
static int sms_call_count = 0;
static int sms_expectations_met = 1;
static int sms_return_value = 0;

void sms_mock_reset(void) {
    sms_expected_phone[0] = '\0';
    sms_expected_message_prefix[0] = '\0';
    sms_call_count = 0;
    sms_expectations_met = 1;
    sms_return_value = 0;
}

void sms_mock_expect(const char *phone, const char *msg_prefix, int ret) {
    snprintf(sms_expected_phone, sizeof(sms_expected_phone), "%s", phone);
    snprintf(sms_expected_message_prefix, sizeof(sms_expected_message_prefix),
             "%s", msg_prefix);
    sms_return_value = ret;
}

int sms_send(const char *phone, const char *message) {
    sms_call_count++;

    // MOCK: verificar expectativas DURANTE la llamada
    if (sms_expected_phone[0] && strcmp(phone, sms_expected_phone) != 0) {
        fprintf(stderr, "SMS MOCK FAIL: expected phone='%s', got '%s'\n",
                sms_expected_phone, phone);
        sms_expectations_met = 0;
    }

    if (sms_expected_message_prefix[0] &&
        strncmp(message, sms_expected_message_prefix,
                strlen(sms_expected_message_prefix)) != 0) {
        fprintf(stderr, "SMS MOCK FAIL: message doesn't start with '%s'\n",
                sms_expected_message_prefix);
        sms_expectations_met = 0;
    }

    return sms_return_value;
}

// =====================================================================
// TESTS
// =====================================================================

static void setup(void) {
    fake_user_db_reset();
    email_spy_reset();
    sms_mock_reset();
    set_stub_time_to_noon();
}

// --- Test con DUMMY (logger) + STUB (clock) ---
static void test_user_not_found(void) {
    setup();
    // No agregamos ningun usuario al fake DB

    NotifyResult r = notify_user(999, "Test", "Hello");
    assert(r == NOTIFY_USER_NOT_FOUND);
    // logger_error fue llamado pero es un DUMMY, no verificamos nada
    printf("  PASS: user not found\n");
}

// --- Test con STUB (clock) ---
static void test_outside_notification_hours(void) {
    setup();
    set_stub_time_to_midnight();  // 2am
    fake_user_db_add(1, "Alice", "alice@test.com", "+1234", true, false);

    NotifyResult r = notify_user(1, "Test", "Hello");
    assert(r == NOTIFY_OK);  // no es error, simplemente no notifica
    assert(email_call_count == 0);  // no se envio email
    printf("  PASS: outside notification hours\n");
}

// --- Test con FAKE (user DB) ---
static void test_no_channels_enabled(void) {
    setup();
    // Crear usuario con ambos canales deshabilitados
    fake_user_db_add(1, "Alice", "alice@test.com", "+1234", false, false);

    NotifyResult r = notify_user(1, "Test", "Hello");
    assert(r == NOTIFY_NO_CHANNELS);
    printf("  PASS: no channels enabled\n");
}

// --- Test con SPY (email) ---
static void test_email_sent_to_correct_address(void) {
    setup();
    fake_user_db_add(1, "Alice", "alice@test.com", "+1234", true, false);

    notify_user(1, "Alert", "Server is down");

    // VERIFICACION SPY: inspeccionar las llamadas grabadas
    assert(email_call_count == 1);
    assert(strcmp(email_calls[0].to, "alice@test.com") == 0);
    assert(strcmp(email_calls[0].subject, "Alert") == 0);
    // Verificar que el body contiene el nombre del usuario
    assert(strstr(email_calls[0].body, "Alice") != NULL);
    assert(strstr(email_calls[0].body, "Server is down") != NULL);

    printf("  PASS: email sent to correct address (spy verified)\n");
}

// --- Test con MOCK (sms) ---
static void test_sms_sent_with_correct_format(void) {
    setup();
    fake_user_db_add(1, "Bob", "bob@test.com", "+5678", false, true);

    // DEFINIR EXPECTATIVAS del mock
    sms_mock_expect("+5678", "Alert:", 0);

    notify_user(1, "Alert", "Server is down");

    // VERIFICAR que las expectativas se cumplieron
    assert(sms_call_count == 1);
    assert(sms_expectations_met);

    printf("  PASS: sms sent with correct format (mock verified)\n");
}

// --- Test con SPY + MOCK (ambos canales) ---
static void test_both_channels_success(void) {
    setup();
    fake_user_db_add(1, "Charlie", "charlie@test.com", "+9999", true, true);
    sms_mock_expect("+9999", "Alert:", 0);

    NotifyResult r = notify_user(1, "Alert", "Disk full");

    assert(r == NOTIFY_OK);
    assert(email_call_count == 1);  // spy
    assert(sms_call_count == 1);    // mock
    assert(sms_expectations_met);   // mock

    printf("  PASS: both channels success\n");
}

// --- Test de partial failure (email falla, sms ok) ---
static void test_partial_failure(void) {
    setup();
    fake_user_db_add(1, "Diana", "diana@test.com", "+1111", true, true);
    email_return_value = -1;  // email fallara
    sms_mock_expect("+1111", "Alert:", 0);

    NotifyResult r = notify_user(1, "Alert", "CPU overload");

    assert(r == NOTIFY_PARTIAL);
    assert(email_call_count == 1);  // intento enviar email (pero fallo)
    assert(sms_call_count == 1);    // SMS si funciono
    assert(sms_expectations_met);

    printf("  PASS: partial failure (email failed, sms ok)\n");
}

// --- Test de all failed ---
static void test_all_channels_failed(void) {
    setup();
    fake_user_db_add(1, "Eve", "eve@test.com", "+2222", true, true);
    email_return_value = -1;
    sms_mock_expect("+2222", "Alert:", -1);  // SMS tambien fallara
    sms_return_value = -1;

    NotifyResult r = notify_user(1, "Alert", "Everything is broken");

    assert(r == NOTIFY_ALL_FAILED);
    printf("  PASS: all channels failed\n");
}

int main(void) {
    printf("=== notification_service tests ===\n");
    printf("  (using: DUMMY=logger, STUB=clock, FAKE=user_db,\n");
    printf("          SPY=email, MOCK=sms)\n\n");

    test_user_not_found();
    test_outside_notification_hours();
    test_no_channels_enabled();
    test_email_sent_to_correct_address();
    test_sms_sent_with_correct_format();
    test_both_channels_success();
    test_partial_failure();
    test_all_channels_failed();

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
```

```bash
# Compilacion:
gcc -o test_notification test_notification_service.c notification_service.c \
    -I include -Wall -Wextra
# Nota: NO linkeamos real_email.c, real_sms.c, real_database.c, real_logger.c
# Los doubles estan definidos directamente en test_notification_service.c

./test_notification
# === notification_service tests ===
#   (using: DUMMY=logger, STUB=clock, FAKE=user_db,
#           SPY=email, MOCK=sms)
#
#   PASS: user not found
#   PASS: outside notification hours
#   PASS: no channels enabled
#   PASS: email sent to correct address (spy verified)
#   PASS: sms sent with correct format (mock verified)
#   PASS: both channels success
#   PASS: partial failure (email failed, sms ok)
#   PASS: all channels failed
#
# === ALL TESTS PASSED ===
```

### 15.3 Diagrama de que double se usa para que

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  MAPA DE DOUBLES EN EL EJEMPLO                                                 │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  notify_user()                                                                  │
│  │                                                                               │
│  ├── user_db_find() ──────── FAKE (array en memoria, busqueda real)             │
│  │                            → Necesitamos logica: agregar + buscar            │
│  │                                                                               │
│  ├── clock_now() ─────────── STUB (retorna valor fijo)                          │
│  │                            → Solo necesitamos controlar la hora              │
│  │                                                                               │
│  ├── logger_info/error() ── DUMMY (ignora, no hace nada)                        │
│  │                            → No nos importa el logging para estos tests      │
│  │                                                                               │
│  ├── email_send() ────────── SPY (graba argumentos, verificacion post-hoc)      │
│  │                            → Queremos ver QUE email se envio                 │
│  │                                                                               │
│  └── sms_send() ─────────── MOCK (expectativas pre-definidas)                   │
│                               → Verificamos que se llamo correctamente          │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 16. Programa de practica — sistema de pagos

### 16.1 Requisitos

Implementa un servicio de procesamiento de pagos con estas dependencias:

```c
// payment_service.h

// --- Dependencias ---

// Gateway de pago (Stripe, PayPal, etc.)
typedef struct {
    char transaction_id[64];
    int status;  // 0=ok, 1=declined, 2=error
} PaymentResult;

int gateway_charge(const char *card_token, int amount_cents, PaymentResult *out);

// Almacen de transacciones
int transaction_store_save(const char *user_id, int amount,
                           const char *tx_id, int status);
int transaction_store_get_daily_total(const char *user_id, int *total);

// Servicio de notificacion
int notify_payment_success(const char *email, int amount_cents,
                           const char *tx_id);
int notify_payment_failure(const char *email, const char *reason);

// Reloj (para verificar limites diarios)
time_t payment_clock_now(void);

// Logger
void payment_log(const char *level, const char *msg);

// --- Servicio ---
typedef enum {
    PAY_OK = 0,
    PAY_DECLINED,
    PAY_DAILY_LIMIT,
    PAY_GATEWAY_ERROR,
    PAY_INVALID_AMOUNT,
} PayStatus;

#define DAILY_LIMIT_CENTS 500000  // $5,000.00

PayStatus process_payment(const char *user_id, const char *email,
                          const char *card_token, int amount_cents);
```

### 16.2 Lo que debes implementar

1. `payment_service.c` — logica del servicio
2. Tests usando los **cinco tipos de doubles**:

| Dependencia | Tipo de double | Justificacion |
|-------------|---------------|---------------|
| `payment_log` | DUMMY | No nos importa el logging |
| `payment_clock_now` | STUB | Controlar la fecha/hora |
| `transaction_store_*` | FAKE | Necesitamos estado (save + get_daily_total) |
| `notify_payment_*` | SPY | Verificar que se envio la notificacion correcta |
| `gateway_charge` | MOCK | Verificar que se llamo con los argumentos correctos |

3. Tests minimos (10):
   - Pago exitoso (importe valido, bajo el limite diario)
   - Pago declinado por el gateway
   - Pago rechazado por limite diario excedido
   - Importe invalido (0, negativo)
   - Error del gateway (status=2)
   - Notificacion de exito enviada con datos correctos
   - Notificacion de fallo enviada
   - Multiples pagos acumulan en el total diario (fake)
   - Gateway llamado con card_token y amount correctos (mock)
   - Transaccion guardada con todos los campos (spy en el store)

---

## 17. Ejercicios

### Ejercicio 1: Identificar el tipo de double

Para cada fragmento de codigo, identifica si es un dummy, stub, fake, spy o mock. Justifica:

```c
// A)
int get_temperature(void) { return 22; }

// B)
static int call_count = 0;
int send_alert(const char *msg) { call_count++; return 0; }

// C)
int db_get(const char *key, char *value) {
    static char store[100][2][64];
    static int count = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(store[i][0], key) == 0) {
            strcpy(value, store[i][1]);
            return 0;
        }
    }
    return -1;
}

// D)
void audit_log(const char *event) { (void)event; }

// E)
int http_post(const char *url, const char *body) {
    assert(strcmp(url, "https://api.example.com/notify") == 0);
    assert(strstr(body, "user_id") != NULL);
    return 200;
}
```

### Ejercicio 2: Convertir stub a spy

Toma el stub de base de datos de la seccion 5.2 y conviertelo en un spy: ademas de retornar valores configurados, debe grabar todas las llamadas con sus argumentos. Escribe tests que verifiquen:
- `db_find_user_by_email` fue llamado exactamente una vez
- El argumento `email` fue "alice@test.com"
- `db_insert_user` fue llamado con un UserRecord cuyo `name` es "Alice"

### Ejercicio 3: Fake de filesystem

Implementa un fake de filesystem en memoria con estas operaciones:

```c
int fake_fs_write(const char *path, const char *data, int len);
int fake_fs_read(const char *path, char *buf, int buf_size);
int fake_fs_exists(const char *path);
int fake_fs_delete(const char *path);
int fake_fs_list(const char *dir, char paths[][256], int max_paths);
```

Usa el fake para testear un modulo de configuracion que lee/escribe archivos `.ini`. Verifica que el fake tiene logica correcta: escribir y luego leer retorna lo mismo.

### Ejercicio 4: Diagrama de doubles para tu proyecto

Elige un modulo de un proyecto real o imaginario. Dibuja un diagrama (como el de la seccion 15.3) que muestre:
- El modulo bajo test en el centro
- Cada dependencia alrededor
- El tipo de double recomendado para cada una
- Justificacion de por que ese tipo y no otro

---

Navegacion:

- Anterior: [S02/T04 — Comparacion de frameworks](../../S02_Frameworks_de_Testing_en_C/T04_Comparacion_de_frameworks/README.md)
- Siguiente: [T02 — Function pointer injection](../T02_Function_pointer_injection/README.md)
- Indice: [BLOQUE 17 — Testing e Ingenieria de Software](../../../BLOQUE_17.md)

---

*Proximo topico: T02 — Function pointer injection*
