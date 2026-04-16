# Link-time substitution — compilar con implementacion mock (mock_file.o en lugar de real_file.o), weak symbols

## 1. El concepto

Link-time substitution es la tecnica de reemplazar una dependencia **en el momento del linkeo**, compilando el test contra un archivo `.o` diferente al de produccion. No necesitas cambiar la API del codigo bajo test (a diferencia de function pointer injection). No necesitas flags especiales del linker (a diferencia de `--wrap`). Solo necesitas **dos implementaciones del mismo header** y elegir cual linkear.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     LINK-TIME SUBSTITUTION                                       │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Produccion:                                                                    │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                       │
│  │ main.c       │    │ user_svc.c   │    │ database.c   │                       │
│  │              │───▶│              │───▶│ (PostgreSQL)  │                       │
│  └──────────────┘    └──────────────┘    └──────────────┘                       │
│                                                                                  │
│  gcc main.o user_svc.o database.o -lpq -o app                                  │
│                                                                                  │
│  Testing:                                                                       │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                       │
│  │ test_user.c  │    │ user_svc.c   │    │ mock_db.c    │                       │
│  │              │───▶│              │───▶│ (array mem)   │                       │
│  └──────────────┘    └──────────────┘    └──────────────┘                       │
│                                                                                  │
│  gcc test_user.o user_svc.o mock_db.o -o test_user                              │
│                                                                                  │
│  user_svc.c es IDENTICO en ambos casos.                                         │
│  Solo cambia contra que .o se linkea.                                            │
│  user_svc.c llama db_insert_user() — el linker decide cual usar.               │
│                                                                                  │
│  ┌────────────────────────────────────────────────────────────────────┐          │
│  │  Ventaja clave: CERO cambios en el codigo de produccion.          │          │
│  │  No hay function pointers, no hay #ifdef, no hay --wrap.         │          │
│  │  Solo un Makefile/CMakeLists.txt diferente.                      │          │
│  └────────────────────────────────────────────────────────────────────┘          │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Como funciona el linker

Para entender link-time substitution, hay que entender que hace el linker:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  COMPILACION Y LINKEO — FASES                                                   │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Fase 1: COMPILACION (gcc -c)                                                   │
│  ┌──────────┐     ┌──────────┐                                                  │
│  │ file.c   │────▶│ file.o   │  Cada .c se compila independientemente           │
│  │          │     │          │  Los simbolos externos quedan como               │
│  │ #include │     │ UNDEF:   │  "referencias sin resolver" (UNDEF)              │
│  │ "db.h"   │     │ db_find  │                                                  │
│  │          │     │ db_insert│                                                  │
│  │ calls:   │     │          │                                                  │
│  │ db_find()│     │ DEFINED: │                                                  │
│  │ db_insert│     │ my_func  │                                                  │
│  └──────────┘     └──────────┘                                                  │
│                                                                                  │
│  Fase 2: LINKEO (gcc file1.o file2.o ... -o executable)                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────┐                               │
│  │ user.o   │  │ mock_db.o│  │ Linker           │                               │
│  │          │  │          │  │                  │                               │
│  │ UNDEF:   │  │ DEFINED: │  │ Resuelve UNDEF:  │                               │
│  │ db_find  │─▶│ db_find  │─▶│ user.o:db_find   │                               │
│  │ db_insert│  │ db_insert│  │ → mock_db.o:addr │                               │
│  │          │  │          │  │                  │                               │
│  └──────────┘  └──────────┘  └──────────────────┘                               │
│                                                                                  │
│  El linker busca cada simbolo UNDEF en los .o que le das.                       │
│  Si le das mock_db.o, encuentra db_find ahi.                                    │
│  Si le das real_db.o, encuentra db_find ahi.                                    │
│  El codigo objeto (user.o) es el mismo en ambos casos.                          │
│                                                                                  │
│  REGLA: El linker NO puede tener DOS definiciones del mismo simbolo.           │
│  Si le das real_db.o Y mock_db.o, error:                                        │
│  "multiple definition of `db_find'"                                             │
│  (Excepto con weak symbols — seccion 8)                                         │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 Anatomia de un .o con nm

```bash
# Ver simbolos de un archivo objeto
$ nm user_service.o
                 U db_find_user_by_email    # U = undefined (necesita resolucion)
                 U db_insert_user           # U = undefined
0000000000000000 T user_service_create      # T = text (definido, exportado)
                 U printf                   # U = de libc

$ nm mock_database.o
0000000000000000 T db_find_user_by_email    # T = definido aqui
0000000000000080 T db_insert_user           # T = definido aqui
0000000000000100 T mock_db_reset            # T = funcion extra del mock

$ nm real_database.o
0000000000000000 T db_find_user_by_email    # T = definido aqui tambien
0000000000000100 T db_insert_user           # T = definido aqui tambien

# El linker ELIGE cual .o provee la definicion:
$ gcc test.o user_service.o mock_database.o -o test    # → usa mock
$ gcc main.o user_service.o real_database.o -lpq -o app # → usa real
```

---

## 2. Ejemplo basico paso a paso

### 2.1 Estructura del proyecto

```
proyecto/
├── include/
│   ├── database.h          ← interfaz (header compartido)
│   └── user_service.h
├── src/
│   ├── database.c          ← implementacion real (PostgreSQL)
│   ├── user_service.c      ← codigo bajo test
│   └── main.c
├── tests/
│   ├── mock_database.c     ← implementacion mock
│   └── test_user_service.c ← tests
└── Makefile
```

### 2.2 El header compartido (contrato)

```c
// include/database.h — LA MISMA interfaz para real y mock
#ifndef DATABASE_H
#define DATABASE_H

typedef struct {
    int id;
    char name[64];
    char email[128];
    char password_hash[128];
} UserRecord;

// Buscar usuario por email. Retorna 0 si encontrado, -1 si no.
int db_find_user_by_email(const char *email, UserRecord *out);

// Insertar usuario. Retorna ID asignado, o -1 si error.
int db_insert_user(const UserRecord *user);

// Eliminar usuario. Retorna 0 si ok, -1 si no encontrado.
int db_delete_user(int user_id);

// Contar usuarios. Retorna el conteo.
int db_count_users(void);

#endif
```

### 2.3 Implementacion real

```c
// src/database.c — implementacion real con PostgreSQL
#include "database.h"
#include <libpq-fe.h>
#include <string.h>
#include <stdio.h>

static PGconn *conn = NULL;

static void ensure_connection(void) {
    if (!conn) {
        conn = PQconnectdb("host=localhost dbname=myapp");
    }
}

int db_find_user_by_email(const char *email, UserRecord *out) {
    ensure_connection();
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT id, name, email, password_hash FROM users WHERE email='%s'",
             email);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return -1;
    }

    out->id = atoi(PQgetvalue(res, 0, 0));
    snprintf(out->name, sizeof(out->name), "%s", PQgetvalue(res, 0, 1));
    snprintf(out->email, sizeof(out->email), "%s", PQgetvalue(res, 0, 2));
    snprintf(out->password_hash, sizeof(out->password_hash), "%s",
             PQgetvalue(res, 0, 3));

    PQclear(res);
    return 0;
}

int db_insert_user(const UserRecord *user) {
    ensure_connection();
    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO users (name, email, password_hash) "
             "VALUES ('%s', '%s', '%s') RETURNING id",
             user->name, user->email, user->password_hash);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return -1;
    }

    int id = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return id;
}

int db_delete_user(int user_id) {
    ensure_connection();
    char query[128];
    snprintf(query, sizeof(query), "DELETE FROM users WHERE id=%d", user_id);

    PGresult *res = PQexec(conn, query);
    int affected = atoi(PQcmdTuples(res));
    PQclear(res);
    return affected > 0 ? 0 : -1;
}

int db_count_users(void) {
    ensure_connection();
    PGresult *res = PQexec(conn, "SELECT COUNT(*) FROM users");
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}
```

### 2.4 Implementacion mock (link-time substitute)

```c
// tests/mock_database.c — implementacion mock (MISMAS funciones, diferente codigo)
#include "database.h"
#include <string.h>
#include <stdio.h>

// --- Estado interno del mock ---
#define MOCK_MAX_USERS 50

static UserRecord mock_users[MOCK_MAX_USERS];
static int mock_user_count = 0;
static int mock_next_id = 1;

// --- Funciones de control (solo para tests) ---
void mock_db_reset(void) {
    mock_user_count = 0;
    mock_next_id = 1;
    memset(mock_users, 0, sizeof(mock_users));
}

void mock_db_seed_user(int id, const char *name, const char *email,
                       const char *pass_hash) {
    if (mock_user_count >= MOCK_MAX_USERS) return;
    UserRecord *u = &mock_users[mock_user_count++];
    u->id = id;
    snprintf(u->name, sizeof(u->name), "%s", name);
    snprintf(u->email, sizeof(u->email), "%s", email);
    snprintf(u->password_hash, sizeof(u->password_hash), "%s", pass_hash);
    if (id >= mock_next_id) mock_next_id = id + 1;
}

int mock_db_get_count(void) {
    return mock_user_count;
}

const UserRecord *mock_db_get_record(int index) {
    if (index < 0 || index >= mock_user_count) return NULL;
    return &mock_users[index];
}

// --- Implementacion de la interfaz database.h ---
// MISMAS FIRMAS que database.c, diferente implementacion

int db_find_user_by_email(const char *email, UserRecord *out) {
    for (int i = 0; i < mock_user_count; i++) {
        if (strcmp(mock_users[i].email, email) == 0) {
            if (out) *out = mock_users[i];
            return 0;
        }
    }
    return -1;
}

int db_insert_user(const UserRecord *user) {
    if (!user) return -1;
    if (mock_user_count >= MOCK_MAX_USERS) return -1;

    // Verificar duplicado
    for (int i = 0; i < mock_user_count; i++) {
        if (strcmp(mock_users[i].email, user->email) == 0) {
            return -1;
        }
    }

    UserRecord *new_user = &mock_users[mock_user_count++];
    *new_user = *user;
    new_user->id = mock_next_id++;
    return new_user->id;
}

int db_delete_user(int user_id) {
    for (int i = 0; i < mock_user_count; i++) {
        if (mock_users[i].id == user_id) {
            // Mover ultimo al hueco
            mock_users[i] = mock_users[mock_user_count - 1];
            mock_user_count--;
            return 0;
        }
    }
    return -1;
}

int db_count_users(void) {
    return mock_user_count;
}
```

### 2.5 Codigo bajo test (sin modificaciones)

```c
// include/user_service.h
#ifndef USER_SERVICE_H
#define USER_SERVICE_H

typedef enum {
    US_OK = 0,
    US_INVALID_INPUT,
    US_DUPLICATE_EMAIL,
    US_DB_ERROR,
} UserServiceError;

int user_service_create(const char *name, const char *email,
                        const char *password);
int user_service_delete(int user_id);
int user_service_exists(const char *email);

#endif
```

```c
// src/user_service.c — codigo de produccion, SIN ninguna referencia a mocks
#include "user_service.h"
#include "database.h"
#include <string.h>
#include <stdio.h>

// Simulacion simplificada de hash (en realidad usarias bcrypt)
static void hash_password(const char *password, char *hash, int hash_size) {
    snprintf(hash, hash_size, "hashed_%s", password);
}

static int validate_email(const char *email) {
    if (!email) return 0;
    const char *at = strchr(email, '@');
    if (!at) return 0;
    if (at == email) return 0;  // nada antes de @
    if (strlen(at + 1) < 3) return 0;  // dominio muy corto
    return 1;
}

int user_service_create(const char *name, const char *email,
                        const char *password) {
    // Validar inputs
    if (!name || strlen(name) == 0) return -US_INVALID_INPUT;
    if (!validate_email(email)) return -US_INVALID_INPUT;
    if (!password || strlen(password) < 8) return -US_INVALID_INPUT;

    // Verificar duplicado
    UserRecord existing;
    if (db_find_user_by_email(email, &existing) == 0) {
        return -US_DUPLICATE_EMAIL;
    }

    // Crear registro
    UserRecord new_user = {0};
    snprintf(new_user.name, sizeof(new_user.name), "%s", name);
    snprintf(new_user.email, sizeof(new_user.email), "%s", email);
    hash_password(password, new_user.password_hash,
                  sizeof(new_user.password_hash));

    // Insertar
    int id = db_insert_user(&new_user);
    if (id < 0) return -US_DB_ERROR;

    return id;
}

int user_service_delete(int user_id) {
    return db_delete_user(user_id);
}

int user_service_exists(const char *email) {
    UserRecord rec;
    return db_find_user_by_email(email, &rec) == 0;
}
```

**Nota critica**: `user_service.c` no tiene `#include "mock_database.h"`, no tiene `#ifdef TEST`, no tiene function pointers. Es codigo de produccion puro. La sustitucion ocurre en el linker.

### 2.6 Tests

```c
// tests/test_user_service.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "user_service.h"
#include "database.h"

// Declaraciones de funciones de control del mock
// (no estan en database.h porque son especificas del mock)
extern void mock_db_reset(void);
extern void mock_db_seed_user(int id, const char *name, const char *email,
                               const char *pass_hash);
extern int mock_db_get_count(void);
extern const UserRecord *mock_db_get_record(int index);

// --- Setup ---
static void setup(void) {
    mock_db_reset();
}

// --- Tests de validacion ---

static void test_create_rejects_null_name(void) {
    setup();
    int rc = user_service_create(NULL, "alice@test.com", "password123");
    assert(rc < 0);
    assert(mock_db_get_count() == 0);  // no se inserto nada
    printf("  PASS: rejects null name\n");
}

static void test_create_rejects_empty_name(void) {
    setup();
    int rc = user_service_create("", "alice@test.com", "password123");
    assert(rc < 0);
    printf("  PASS: rejects empty name\n");
}

static void test_create_rejects_invalid_email(void) {
    setup();
    assert(user_service_create("Alice", "not-an-email", "password123") < 0);
    assert(user_service_create("Alice", "@test.com", "password123") < 0);
    assert(user_service_create("Alice", "alice@", "password123") < 0);
    assert(user_service_create("Alice", NULL, "password123") < 0);
    printf("  PASS: rejects invalid email\n");
}

static void test_create_rejects_short_password(void) {
    setup();
    int rc = user_service_create("Alice", "alice@test.com", "short");
    assert(rc < 0);
    printf("  PASS: rejects short password\n");
}

// --- Tests de logica ---

static void test_create_user_success(void) {
    setup();
    int id = user_service_create("Alice", "alice@test.com", "password123");
    assert(id > 0);
    assert(mock_db_get_count() == 1);

    // Verificar que se guardo correctamente
    const UserRecord *rec = mock_db_get_record(0);
    assert(rec != NULL);
    assert(strcmp(rec->name, "Alice") == 0);
    assert(strcmp(rec->email, "alice@test.com") == 0);
    // Verificar que la password fue hasheada (no en texto plano)
    assert(strcmp(rec->password_hash, "password123") != 0);
    assert(strstr(rec->password_hash, "hashed_") != NULL);

    printf("  PASS: create user success\n");
}

static void test_create_duplicate_email(void) {
    setup();
    mock_db_seed_user(1, "Existing", "alice@test.com", "hash");

    int rc = user_service_create("Alice", "alice@test.com", "password123");
    assert(rc == -US_DUPLICATE_EMAIL);
    assert(mock_db_get_count() == 1);  // no se agrego otro

    printf("  PASS: create duplicate email rejected\n");
}

static void test_create_multiple_users(void) {
    setup();
    int id1 = user_service_create("Alice", "alice@test.com", "password123");
    int id2 = user_service_create("Bob", "bob@test.com", "password456");
    int id3 = user_service_create("Charlie", "charlie@test.com", "password789");

    assert(id1 > 0 && id2 > 0 && id3 > 0);
    assert(id1 != id2 && id2 != id3);
    assert(mock_db_get_count() == 3);

    printf("  PASS: create multiple users\n");
}

static void test_delete_existing_user(void) {
    setup();
    mock_db_seed_user(42, "Alice", "alice@test.com", "hash");

    int rc = user_service_delete(42);
    assert(rc == 0);
    assert(mock_db_get_count() == 0);

    printf("  PASS: delete existing user\n");
}

static void test_delete_nonexistent_user(void) {
    setup();
    int rc = user_service_delete(999);
    assert(rc == -1);

    printf("  PASS: delete nonexistent user\n");
}

static void test_exists_true(void) {
    setup();
    mock_db_seed_user(1, "Alice", "alice@test.com", "hash");

    assert(user_service_exists("alice@test.com") == 1);

    printf("  PASS: exists true\n");
}

static void test_exists_false(void) {
    setup();
    assert(user_service_exists("nobody@test.com") == 0);

    printf("  PASS: exists false\n");
}

int main(void) {
    printf("=== user_service tests (link-time substitution) ===\n");
    test_create_rejects_null_name();
    test_create_rejects_empty_name();
    test_create_rejects_invalid_email();
    test_create_rejects_short_password();
    test_create_user_success();
    test_create_duplicate_email();
    test_create_multiple_users();
    test_delete_existing_user();
    test_delete_nonexistent_user();
    test_exists_true();
    test_exists_false();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

### 2.7 Makefile

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -I include

# --- Produccion ---
app: src/main.o src/user_service.o src/database.o
	$(CC) $^ -lpq -o $@

# --- Tests ---
# NOTA: linkeamos con mock_database.o EN LUGAR DE database.o
test_user_service: tests/test_user_service.o src/user_service.o tests/mock_database.o
	$(CC) $^ -o $@

# --- Compilacion ---
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Targets ---
.PHONY: test clean

test: test_user_service
	./test_user_service

clean:
	rm -f src/*.o tests/*.o app test_user_service
```

```bash
$ make test
gcc -Wall -Wextra -Wpedantic -std=c11 -I include -c tests/test_user_service.c -o tests/test_user_service.o
gcc -Wall -Wextra -Wpedantic -std=c11 -I include -c src/user_service.c -o src/user_service.o
gcc -Wall -Wextra -Wpedantic -std=c11 -I include -c tests/mock_database.c -o tests/mock_database.o
gcc tests/test_user_service.o src/user_service.o tests/mock_database.o -o test_user_service
./test_user_service
=== user_service tests (link-time substitution) ===
  PASS: rejects null name
  PASS: rejects empty name
  PASS: rejects invalid email
  PASS: rejects short password
  PASS: create user success
  PASS: create duplicate email rejected
  PASS: create multiple users
  PASS: delete existing user
  PASS: delete nonexistent user
  PASS: exists true
  PASS: exists false
=== ALL TESTS PASSED ===
```

---

## 3. La clave — el header como contrato

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  EL HEADER ES EL CONTRATO                                                       │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│                    database.h (contrato)                                         │
│                    ┌──────────────────────────┐                                  │
│                    │ int db_find_user(...)     │                                  │
│                    │ int db_insert_user(...)   │                                  │
│                    │ int db_delete_user(...)   │                                  │
│                    │ int db_count_users(...)   │                                  │
│                    └─────────┬────────────────┘                                  │
│                              │                                                   │
│              ┌───────────────┼───────────────┐                                   │
│              │               │               │                                   │
│              ▼               ▼               ▼                                   │
│  ┌──────────────────┐ ┌──────────────┐ ┌──────────────┐                         │
│  │   database.c     │ │ mock_db.c    │ │ stub_db.c    │                         │
│  │   (PostgreSQL)   │ │ (in-memory)  │ │ (return -1)  │                         │
│  │                  │ │              │ │              │                         │
│  │  Produccion      │ │  Fake        │ │  Stub        │                         │
│  └──────────────────┘ └──────────────┘ └──────────────┘                         │
│                                                                                  │
│  Regla: TODAS las implementaciones deben respetar las firmas del header.        │
│  Si el header cambia, TODAS las implementaciones deben actualizarse.            │
│  El compilador verifica que cada .c implementa las firmas correctas.            │
│                                                                                  │
│  Regla: un mock PUEDE tener funciones adicionales (mock_db_reset, etc.)         │
│  que no estan en el header. Estas solo se usan en los tests.                    │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Translation units y One Definition Rule (ODR)

### 4.1 Translation unit

Cada archivo `.c` + sus `#include`s forman una **translation unit**. El compilador procesa cada translation unit independientemente, produciendo un `.o`:

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  TRANSLATION UNITS                                                               │
│                                                                                  │
│  TU 1: user_service.c                                                           │
│  ├── #include "user_service.h"                                                  │
│  └── #include "database.h"     ← ve las DECLARACIONES de db_find, etc.         │
│      → Compila a user_service.o                                                 │
│      → db_find es UNDEFINED (U) en user_service.o                               │
│                                                                                  │
│  TU 2: database.c              ← implementacion real                            │
│  └── #include "database.h"                                                      │
│      → Compila a database.o                                                     │
│      → db_find es DEFINED (T) en database.o                                     │
│                                                                                  │
│  TU 3: mock_database.c         ← implementacion mock                            │
│  └── #include "database.h"                                                      │
│      → Compila a mock_database.o                                                │
│      → db_find es DEFINED (T) en mock_database.o                                │
│                                                                                  │
│  Linkeo para test:                                                               │
│  user_service.o (db_find=U) + mock_database.o (db_find=T)                       │
│  → Linker resuelve db_find → mock_database.o                                   │
│                                                                                  │
│  Linkeo para produccion:                                                        │
│  user_service.o (db_find=U) + database.o (db_find=T)                            │
│  → Linker resuelve db_find → database.o                                        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 One Definition Rule (ODR)

```
El linker exige que cada simbolo tenga EXACTAMENTE UNA definicion
(excepto con weak symbols — seccion 8).

Si linkeas user_service.o + database.o + mock_database.o:
→ ERROR: "multiple definition of `db_find_user_by_email'"

Por eso debes elegir UNO: database.o O mock_database.o, nunca ambos.
```

### 4.3 Funciones static y link-time substitution

Las funciones `static` son **locales a su translation unit**. No participan en el linkeo. No puedes sustituirlas con link-time substitution:

```c
// database.c
static int validate_query(const char *sql) {
    // Esta funcion es static: solo visible dentro de database.c
    // NO puedes reemplazarla desde fuera
}

int db_find_user_by_email(const char *email, UserRecord *out) {
    // Esta SI es sustituible (tiene linkage externo)
    char query[256];
    if (!validate_query(query)) return -1;  // llama a la static local
    // ...
}
```

Para testear funciones static:
- Opcion 1: `#include "database.c"` directamente (incluir el .c en el test)
- Opcion 2: hacer la funcion no-static (exponerla en el header)
- Opcion 3: usar compilacion condicional (`#ifdef TEST` para exponer)

---

## 5. Multiples mocks en un proyecto

### 5.1 Proyecto con multiples dependencias

```
proyecto/
├── include/
│   ├── database.h
│   ├── email.h
│   ├── cache.h
│   └── user_service.h
├── src/
│   ├── database.c      ← real PostgreSQL
│   ├── email.c          ← real SMTP
│   ├── cache.c          ← real Redis
│   └── user_service.c   ← codigo bajo test
├── tests/
│   ├── mocks/
│   │   ├── mock_database.c
│   │   ├── mock_email.c
│   │   └── mock_cache.c
│   └── test_user_service.c
└── Makefile
```

### 5.2 Makefile para multiples mocks

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I include

# Produccion: todos los .c reales
APP_SRCS = src/main.c src/user_service.c src/database.c src/email.c src/cache.c
app: $(APP_SRCS:.c=.o)
	$(CC) $^ -lpq -lhiredis -o $@

# Tests: user_service.c real + mocks de dependencias
TEST_USER_SRCS = tests/test_user_service.c \
                 src/user_service.c \
                 tests/mocks/mock_database.c \
                 tests/mocks/mock_email.c \
                 tests/mocks/mock_cache.c

test_user_service: $(TEST_USER_SRCS:.c=.o)
	$(CC) $^ -o $@

# Puedes testear database.c contra un mock de la red:
# TEST_DB_SRCS = tests/test_database.c src/database.c tests/mocks/mock_network.c

.PHONY: test
test: test_user_service
	./test_user_service
```

### 5.3 Mezclar real y mock

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  MEZCLAR REAL Y MOCK                                                            │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Puedes linkear ALGUNOS modulos reales y OTROS mock:                            │
│                                                                                  │
│  Test de user_service:                                                          │
│  ┌─────────────────────┐                                                        │
│  │ user_service.o REAL │                                                        │
│  │ mock_database.o     │ ← mock (no queremos PostgreSQL)                        │
│  │ mock_email.o        │ ← mock (no queremos SMTP)                              │
│  │ cache.o       REAL  │ ← real (Redis, si queremos test de integracion)        │
│  └─────────────────────┘                                                        │
│                                                                                  │
│  Esto es muy flexible: decides POR MODULO que es real y que es mock.            │
│                                                                                  │
│  Limitacion: si database.c depende internamente de otra funcion                 │
│  (ej: db_connect()), esa funcion tambien necesita estar definida                │
│  en alguno de los .o que linkeas. Los mocks son "transitivos".                  │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Tipos de mock para link-time substitution

### 6.1 Stub (retorna valor fijo)

El mas simple. Cada funcion retorna un valor hardcodeado:

```c
// tests/mocks/stub_database.c
#include "database.h"

int db_find_user_by_email(const char *email, UserRecord *out) {
    (void)email; (void)out;
    return -1;  // siempre "no encontrado"
}

int db_insert_user(const UserRecord *user) {
    (void)user;
    return 1;  // siempre retorna ID=1
}

int db_delete_user(int user_id) {
    (void)user_id;
    return 0;  // siempre exito
}

int db_count_users(void) {
    return 0;  // siempre vacio
}
```

### 6.2 Stub configurable

Permite cambiar el valor de retorno desde el test:

```c
// tests/mocks/configurable_stub_database.c
#include "database.h"

static int find_return = -1;
static int insert_return = 1;
static int delete_return = 0;
static int count_return = 0;

void stub_db_configure(int find_rc, int insert_rc, int delete_rc, int count_rc) {
    find_return = find_rc;
    insert_return = insert_rc;
    delete_return = delete_rc;
    count_return = count_rc;
}

void stub_db_reset(void) {
    find_return = -1;
    insert_return = 1;
    delete_return = 0;
    count_return = 0;
}

int db_find_user_by_email(const char *email, UserRecord *out) {
    (void)email; (void)out;
    return find_return;
}

int db_insert_user(const UserRecord *user) {
    (void)user;
    return insert_return;
}

int db_delete_user(int user_id) {
    (void)user_id;
    return delete_return;
}

int db_count_users(void) {
    return count_return;
}
```

### 6.3 Fake (implementacion funcional)

Lo que mostramos en la seccion 2.4 — un mock con logica real simplificada.

### 6.4 Spy (graba llamadas)

```c
// tests/mocks/spy_database.c
#include "database.h"
#include <string.h>

#define SPY_MAX_CALLS 32

// --- Registro de llamadas ---
typedef struct {
    char function[64];
    char arg_email[128];
    int arg_user_id;
    UserRecord arg_user;
} SpyCall;

static SpyCall spy_calls[SPY_MAX_CALLS];
static int spy_call_count = 0;

// --- Configurar retornos ---
static int find_return = -1;
static UserRecord find_record = {0};
static int insert_return = 1;

void spy_db_reset(void) {
    spy_call_count = 0;
    find_return = -1;
    insert_return = 1;
    memset(&find_record, 0, sizeof(find_record));
    memset(spy_calls, 0, sizeof(spy_calls));
}

void spy_db_set_find_result(int rc, const UserRecord *rec) {
    find_return = rc;
    if (rec) find_record = *rec;
}

void spy_db_set_insert_result(int rc) {
    insert_return = rc;
}

int spy_db_call_count(void) {
    return spy_call_count;
}

const SpyCall *spy_db_get_call(int index) {
    if (index < 0 || index >= spy_call_count) return NULL;
    return &spy_calls[index];
}

int spy_db_was_called(const char *function_name) {
    for (int i = 0; i < spy_call_count; i++) {
        if (strcmp(spy_calls[i].function, function_name) == 0) return 1;
    }
    return 0;
}

// --- Implementaciones spy ---

int db_find_user_by_email(const char *email, UserRecord *out) {
    if (spy_call_count < SPY_MAX_CALLS) {
        SpyCall *c = &spy_calls[spy_call_count++];
        snprintf(c->function, sizeof(c->function), "db_find_user_by_email");
        snprintf(c->arg_email, sizeof(c->arg_email), "%s", email ? email : "");
    }
    if (find_return == 0 && out) *out = find_record;
    return find_return;
}

int db_insert_user(const UserRecord *user) {
    if (spy_call_count < SPY_MAX_CALLS) {
        SpyCall *c = &spy_calls[spy_call_count++];
        snprintf(c->function, sizeof(c->function), "db_insert_user");
        if (user) c->arg_user = *user;
    }
    return insert_return;
}

int db_delete_user(int user_id) {
    if (spy_call_count < SPY_MAX_CALLS) {
        SpyCall *c = &spy_calls[spy_call_count++];
        snprintf(c->function, sizeof(c->function), "db_delete_user");
        c->arg_user_id = user_id;
    }
    return 0;
}

int db_count_users(void) {
    if (spy_call_count < SPY_MAX_CALLS) {
        SpyCall *c = &spy_calls[spy_call_count++];
        snprintf(c->function, sizeof(c->function), "db_count_users");
    }
    return 0;
}
```

```c
// En el test:
static void test_create_hashes_password(void) {
    spy_db_reset();
    spy_db_set_find_result(-1, NULL);  // no existe
    spy_db_set_insert_result(1);       // insert OK

    user_service_create("Alice", "alice@test.com", "mypassword");

    // Verificar con el SPY que db_insert fue llamado
    assert(spy_db_was_called("db_insert_user"));

    // Verificar que la password fue hasheada
    const SpyCall *call = spy_db_get_call(1);  // segunda llamada = insert
    assert(strcmp(call->arg_user.password_hash, "mypassword") != 0);
    assert(strstr(call->arg_user.password_hash, "hashed_") != NULL);

    printf("  PASS: password was hashed before insert\n");
}
```

---

## 7. CMake integration

### 7.1 CMakeLists.txt con link-time substitution

```cmake
cmake_minimum_required(VERSION 3.14)
project(link_time_example C)
set(CMAKE_C_STANDARD 11)

# --- Libreria de produccion ---
add_library(user_service src/user_service.c)
target_include_directories(user_service PUBLIC include)
# NOTA: user_service NO linkea con database_real
# Es responsabilidad del ejecutable final elegir la implementacion

# --- Implementacion real de database ---
add_library(database_real src/database.c)
target_include_directories(database_real PUBLIC include)
target_link_libraries(database_real PRIVATE pq)  # libpq para PostgreSQL

# --- Mock de database ---
add_library(database_mock tests/mocks/mock_database.c)
target_include_directories(database_mock PUBLIC include)

# --- Spy de database ---
add_library(database_spy tests/mocks/spy_database.c)
target_include_directories(database_spy PUBLIC include)

# --- Aplicacion de produccion ---
add_executable(app src/main.c)
target_link_libraries(app PRIVATE user_service database_real)

# --- Tests ---
enable_testing()

# Test con mock (fake)
add_executable(test_user_service tests/test_user_service.c)
target_link_libraries(test_user_service PRIVATE user_service database_mock)
add_test(NAME test_user_service COMMAND test_user_service)
set_tests_properties(test_user_service PROPERTIES LABELS "unit")

# Test con spy
add_executable(test_user_service_spy tests/test_user_service_spy.c)
target_link_libraries(test_user_service_spy PRIVATE user_service database_spy)
add_test(NAME test_user_service_spy COMMAND test_user_service_spy)
set_tests_properties(test_user_service_spy PROPERTIES LABELS "unit;spy")
```

### 7.2 Funcion helper para CMake

```cmake
# Helper: crear un test con mocks especificos
function(add_mocked_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE include)

    # Los argumentos extra son las librerias a linkear
    # (codigo real + mocks)
    target_link_libraries(${TEST_NAME} PRIVATE ${ARGN})

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "unit"
        TIMEOUT 10
    )
endfunction()

# Uso:
add_mocked_test(test_user
    tests/test_user_service.c
    user_service          # real
    database_mock         # mock
    email_mock            # mock
    cache_mock            # mock
)

add_mocked_test(test_integration
    tests/test_integration.c
    user_service          # real
    database_real         # real (necesita PostgreSQL)
    email_mock            # mock (no enviar emails reales)
    cache_real            # real (necesita Redis)
)
```

---

## 8. Weak symbols — override sin conflicto

### 8.1 El problema del ODR

Con link-time substitution normal, no puedes tener la implementacion real Y la mock en el mismo ejecutable. El linker da error por "multiple definition". Los **weak symbols** resuelven esto.

### 8.2 Que son los weak symbols

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  WEAK vs STRONG SYMBOLS                                                         │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  STRONG symbol (default):                                                       │
│  int db_find_user_by_email(...) { /* ... */ }                                   │
│  → Solo puede existir UNA definicion. Dos .o con el mismo → error.             │
│                                                                                  │
│  WEAK symbol:                                                                   │
│  __attribute__((weak))                                                          │
│  int db_find_user_by_email(...) { /* ... */ }                                   │
│  → Puede ser reemplazado por una definicion STRONG.                             │
│  → Si hay una strong, el linker usa la strong.                                  │
│  → Si no hay strong, el linker usa la weak.                                     │
│  → Si hay dos weak y ninguna strong, el linker elige una (no es error).         │
│                                                                                  │
│  Analogia:                                                                      │
│  • Strong = "YO soy la definicion, punto"                                       │
│  • Weak = "Yo puedo ser la definicion, pero si alguien mejor aparece, cedan"   │
│                                                                                  │
│  Soporte: GCC, Clang, ICC (no estandar C, pero universal en practica)          │
│  No soportado: MSVC (Windows usa __declspec(selectany) como alternativa)        │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 8.3 Uso en testing

```c
// database.c — implementacion real, marcada como WEAK
#include "database.h"
#include <libpq-fe.h>

__attribute__((weak))
int db_find_user_by_email(const char *email, UserRecord *out) {
    // Implementacion real con PostgreSQL
    // ...
    return 0;
}

__attribute__((weak))
int db_insert_user(const UserRecord *user) {
    // Implementacion real
    // ...
    return 42;
}
```

```c
// tests/mock_database.c — implementacion mock, STRONG (default)
#include "database.h"
#include <string.h>

// Sin __attribute__((weak)) → es STRONG por defecto
// El linker la preferira sobre la version weak de database.c

int db_find_user_by_email(const char *email, UserRecord *out) {
    // Implementacion mock
    return -1;
}

int db_insert_user(const UserRecord *user) {
    // Implementacion mock
    return 1;
}
```

```bash
# Ahora puedes linkear AMBOS y el linker elige la strong (mock):
gcc test.o user_service.o database.o mock_database.o -o test
# No hay error! El linker usa mock_database.o para db_find y db_insert
# porque son strong, y descarta las weak de database.o

# En produccion (sin mock_database.o), usa las weak:
gcc main.o user_service.o database.o -lpq -o app
# El linker usa database.o (las unicas definiciones disponibles)
```

### 8.4 Verificar weak symbols con nm

```bash
$ nm database.o
0000000000000000 W db_find_user_by_email    # W = weak
0000000000000080 W db_insert_user           # W = weak

$ nm mock_database.o
0000000000000000 T db_find_user_by_email    # T = strong (text)
0000000000000060 T db_insert_user           # T = strong
```

### 8.5 Weak individual — override parcial

La ventaja principal de weak symbols: puedes overridear ALGUNAS funciones y dejar otras con la implementacion real:

```c
// database.c — TODAS las funciones son weak
__attribute__((weak)) int db_find_user_by_email(...) { /* real PG */ }
__attribute__((weak)) int db_insert_user(...)         { /* real PG */ }
__attribute__((weak)) int db_delete_user(...)         { /* real PG */ }
__attribute__((weak)) int db_count_users(void)        { /* real PG */ }
```

```c
// tests/mock_find_only.c — solo override de find
// Las demas funciones usan la implementacion real (weak)

int db_find_user_by_email(const char *email, UserRecord *out) {
    // Mock: siempre retorna "usuario existe"
    out->id = 1;
    snprintf(out->name, sizeof(out->name), "MockUser");
    snprintf(out->email, sizeof(out->email), "%s", email);
    return 0;
}

// db_insert_user, db_delete_user, db_count_users:
// NO definidos aqui → el linker usa las versiones weak de database.o
```

```bash
# Linkear ambos:
gcc test.o user_service.o database.o mock_find_only.o -lpq -o test
# db_find_user_by_email → mock_find_only.o (strong)
# db_insert_user → database.o (weak, la unica disponible)
# db_delete_user → database.o (weak)
# db_count_users → database.o (weak)
```

### 8.6 Macro helper para weak

```c
// helper para marcar funciones como weak
#ifdef ENABLE_WEAK_SYMBOLS
#define WEAKFN __attribute__((weak))
#else
#define WEAKFN
#endif

// database.c:
WEAKFN int db_find_user_by_email(const char *email, UserRecord *out) {
    // ...
}
```

```bash
# Compilar con weak habilitado:
gcc -DENABLE_WEAK_SYMBOLS -c database.c -o database.o
# Ahora database.o tiene simbolos weak

# Compilar sin weak (normal, para produccion):
gcc -c database.c -o database.o
# Ahora database.o tiene simbolos strong (default)
```

### 8.7 Weak symbols: cuidados

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  WEAK SYMBOLS — CUIDADOS                                                        │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  1. No son parte del estandar C                                                 │
│     → __attribute__((weak)) es extension GCC/Clang                              │
│     → No funciona en MSVC (Windows)                                             │
│     → Para portabilidad, usar #ifdef __GNUC__                                   │
│                                                                                  │
│  2. Static libraries (.a) pueden tener comportamiento inesperado                │
│     → El linker extrae .o de .a solo si necesita resolver un simbolo            │
│     → Si el simbolo ya esta resuelto (por un weak), puede no extraer el .o     │
│     → Resultado: la version strong del .a no se usa                             │
│     → Solucion: usar --whole-archive para forzar la extraccion completa        │
│                                                                                  │
│  3. Con LTO (Link-Time Optimization) el comportamiento puede cambiar           │
│     → LTO puede eliminar funciones weak "no usadas"                             │
│     → Agregar __attribute__((used)) ademas de weak si usas LTO                 │
│                                                                                  │
│  4. Dos weak symbols del mismo nombre: el linker elige uno arbitrariamente      │
│     → Puede causar bugs silenciosos si dos mocks definen el mismo simbolo      │
│     → Siempre tener MAXIMO una definicion por simbolo en el linkeo              │
│                                                                                  │
│  5. El orden de los .o en la linea de comando importa con librerias             │
│     → gcc test.o mock.o -lrealdb: el linker ve mock.o primero                  │
│     → gcc test.o -lrealdb mock.o: puede haber conflictos                       │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Default implementations con weak symbols

Un patron poderoso: proveer implementaciones default que los tests pueden overridear:

```c
// logging.h
#ifndef LOGGING_H
#define LOGGING_H
void log_info(const char *msg);
void log_error(const char *msg);
void log_debug(const char *msg);
#endif

// logging.c — implementaciones default (weak)
#include "logging.h"
#include <stdio.h>

__attribute__((weak))
void log_info(const char *msg) {
    fprintf(stdout, "[INFO] %s\n", msg);
}

__attribute__((weak))
void log_error(const char *msg) {
    fprintf(stderr, "[ERROR] %s\n", msg);
}

__attribute__((weak))
void log_debug(const char *msg) {
    // Default: no hacer nada (debug deshabilitado)
    (void)msg;
}
```

```c
// En un test: silenciar TODA la salida de logging
// tests/silent_logging.c

#include "logging.h"

void log_info(const char *msg) { (void)msg; }
void log_error(const char *msg) { (void)msg; }
void log_debug(const char *msg) { (void)msg; }
// Strong → reemplazan las weak → silencio total
```

```c
// En otro test: capturar los mensajes de error
// tests/capture_logging.c

#include "logging.h"
#include <string.h>

#define MAX_LOG_ENTRIES 100
static char error_log[MAX_LOG_ENTRIES][256];
static int error_count = 0;

void log_error(const char *msg) {
    if (error_count < MAX_LOG_ENTRIES) {
        snprintf(error_log[error_count++], 256, "%s", msg);
    }
}

// log_info y log_debug: usar las weak defaults (no override)

int get_error_count(void) { return error_count; }
const char *get_error_message(int index) {
    return (index >= 0 && index < error_count) ? error_log[index] : NULL;
}
void reset_error_log(void) { error_count = 0; }
```

---

## 10. Patron: un header de mock adicional

Para funciones de control del mock (reset, seed, etc.), crea un header separado que solo los tests usan:

```c
// tests/mocks/mock_database.h — solo para tests
#ifndef MOCK_DATABASE_H
#define MOCK_DATABASE_H

#include "database.h"

// Funciones de control del mock
void mock_db_reset(void);
void mock_db_seed_user(int id, const char *name, const char *email,
                       const char *pass_hash);
int mock_db_get_count(void);
const UserRecord *mock_db_get_record(int index);

// Para spy mode
int mock_db_call_count(const char *function_name);
const char *mock_db_last_email_arg(void);

#endif
```

```c
// tests/test_user_service.c
#include "user_service.h"
#include "database.h"            // interfaz real
#include "mocks/mock_database.h" // funciones de control del mock

static void test_example(void) {
    mock_db_reset();
    mock_db_seed_user(1, "Alice", "alice@test.com", "hash");
    // ...
}
```

El codigo de produccion NUNCA incluye `mock_database.h`. Solo el test lo conoce.

---

## 11. Comparacion con Rust y Go

### 11.1 Rust — no necesita link-time substitution

```rust
// Rust no usa link-time substitution para testing.
// El sistema de traits + generics resuelve el problema en compile time.

trait Database {
    fn find_user(&self, email: &str) -> Option<User>;
    fn insert_user(&self, user: &User) -> Result<i32, Error>;
}

// El equivalente mas cercano en Rust seria "conditional compilation":
#[cfg(not(test))]
fn get_database() -> PostgresDB { /* real */ }

#[cfg(test)]
fn get_database() -> FakeDB { /* fake */ }

// Pero esto es raro en Rust. Lo normal es inyeccion por parametro (generics).

// Rust SI tiene weak symbols, pero se usan para otros propositos
// (interop con C, plugins), no para testing.
```

### 11.2 Go — build tags (equivalente conceptual)

```go
// Go tiene "build tags" que son conceptualmente similares
// a link-time substitution:

// database_production.go
//go:build !testing
package myapp

func dbFindUser(email string) (*User, error) {
    // PostgreSQL real
}

// database_test_impl.go
//go:build testing
package myapp

func dbFindUser(email string) (*User, error) {
    // In-memory fake
}

// Pero la practica idomiatica en Go es inyeccion por interfaz,
// no build tags para testing.
```

### 11.3 Tabla comparativa

| Aspecto | C (link-time subst.) | Rust | Go |
|---------|---------------------|------|-----|
| Mecanismo | Elegir .o en linkeo | Traits + generics | Interfaces |
| Cambia el source | No | No | No |
| Cambia el build | Si (diferente .o) | No (traits son compile-time) | No (interfaces son runtime) |
| Override parcial | Weak symbols | No aplica | No aplica |
| Verificacion | Linker time | Compile time | Compile time |
| Overhead runtime | 0 | 0 (static dispatch) | ~1-3 ns (interface) |
| Portabilidad | Buena (weak: solo GCC/Clang) | Excelente | Excelente |
| Idiomatico | Si, para C | No (usar traits) | No (usar interfaces) |

---

## 12. Ejemplo completo — sistema de archivos con mock

### 12.1 Interfaz

```c
// include/filesystem.h
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>

int fs_read_file(const char *path, char *buf, int buf_size);
int fs_write_file(const char *path, const char *data, int len);
bool fs_exists(const char *path);
int fs_delete(const char *path);
int fs_list_dir(const char *dir, char names[][256], int max_names);
long fs_file_size(const char *path);

#endif
```

### 12.2 Implementacion real

```c
// src/filesystem.c
#include "filesystem.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

int fs_read_file(const char *path, char *buf, int buf_size) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int n = fread(buf, 1, buf_size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return n;
}

int fs_write_file(const char *path, const char *data, int len) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int written = fwrite(data, 1, len, f);
    fclose(f);
    return written;
}

bool fs_exists(const char *path) {
    return access(path, F_OK) == 0;
}

int fs_delete(const char *path) {
    return unlink(path) == 0 ? 0 : -1;
}

int fs_list_dir(const char *dir, char names[][256], int max_names) {
    DIR *d = opendir(dir);
    if (!d) return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < max_names) {
        if (entry->d_name[0] == '.') continue;
        snprintf(names[count], 256, "%s", entry->d_name);
        count++;
    }
    closedir(d);
    return count;
}

long fs_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}
```

### 12.3 Mock de filesystem en memoria

```c
// tests/mocks/mock_filesystem.c
#include "filesystem.h"
#include <string.h>
#include <stdio.h>

#define MOCK_FS_MAX_FILES 64
#define MOCK_FS_MAX_CONTENT 4096

typedef struct {
    char path[256];
    char content[MOCK_FS_MAX_CONTENT];
    int size;
    bool active;
} MockFile;

static MockFile mock_files[MOCK_FS_MAX_FILES];
static int mock_file_count = 0;

// --- Control ---
void mock_fs_reset(void) {
    mock_file_count = 0;
    memset(mock_files, 0, sizeof(mock_files));
}

void mock_fs_add_file(const char *path, const char *content) {
    if (mock_file_count >= MOCK_FS_MAX_FILES) return;
    MockFile *f = &mock_files[mock_file_count++];
    f->active = true;
    snprintf(f->path, sizeof(f->path), "%s", path);
    int len = strlen(content);
    memcpy(f->content, content, len);
    f->size = len;
}

int mock_fs_count(void) {
    int count = 0;
    for (int i = 0; i < mock_file_count; i++) {
        if (mock_files[i].active) count++;
    }
    return count;
}

// --- Helpers ---
static MockFile *find_file(const char *path) {
    for (int i = 0; i < mock_file_count; i++) {
        if (mock_files[i].active && strcmp(mock_files[i].path, path) == 0) {
            return &mock_files[i];
        }
    }
    return NULL;
}

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int starts_with(const char *str, const char *prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

// --- Implementacion de filesystem.h ---

int fs_read_file(const char *path, char *buf, int buf_size) {
    MockFile *f = find_file(path);
    if (!f) return -1;
    int to_copy = f->size < buf_size - 1 ? f->size : buf_size - 1;
    memcpy(buf, f->content, to_copy);
    buf[to_copy] = '\0';
    return to_copy;
}

int fs_write_file(const char *path, const char *data, int len) {
    MockFile *f = find_file(path);
    if (f) {
        // Overwrite
        memcpy(f->content, data, len);
        f->size = len;
        return len;
    }
    // Crear nuevo
    if (mock_file_count >= MOCK_FS_MAX_FILES) return -1;
    f = &mock_files[mock_file_count++];
    f->active = true;
    snprintf(f->path, sizeof(f->path), "%s", path);
    memcpy(f->content, data, len);
    f->size = len;
    return len;
}

bool fs_exists(const char *path) {
    return find_file(path) != NULL;
}

int fs_delete(const char *path) {
    MockFile *f = find_file(path);
    if (!f) return -1;
    f->active = false;
    return 0;
}

int fs_list_dir(const char *dir, char names[][256], int max_names) {
    int count = 0;
    int dir_len = strlen(dir);

    for (int i = 0; i < mock_file_count && count < max_names; i++) {
        if (!mock_files[i].active) continue;
        if (!starts_with(mock_files[i].path, dir)) continue;

        // El path debe ser dir/filename (sin subdirectorios)
        const char *rest = mock_files[i].path + dir_len;
        if (*rest == '/') rest++;
        if (strchr(rest, '/') != NULL) continue;  // subdirectorio

        snprintf(names[count], 256, "%s", rest);
        count++;
    }
    return count;
}

long fs_file_size(const char *path) {
    MockFile *f = find_file(path);
    return f ? f->size : -1;
}
```

### 12.4 Codigo bajo test — config parser

```c
// include/config.h
#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_MAX_ENTRIES 64

typedef struct {
    char key[64];
    char value[256];
} ConfigEntry;

typedef struct {
    ConfigEntry entries[CONFIG_MAX_ENTRIES];
    int count;
    char filepath[256];
} Config;

int config_load(Config *cfg, const char *filepath);
int config_save(const Config *cfg);
const char *config_get(const Config *cfg, const char *key);
int config_set(Config *cfg, const char *key, const char *value);
int config_delete(Config *cfg, const char *key);

#endif
```

```c
// src/config.c — usa filesystem.h, no sabe si es real o mock
#include "config.h"
#include "filesystem.h"
#include <string.h>
#include <stdio.h>

int config_load(Config *cfg, const char *filepath) {
    if (!cfg || !filepath) return -1;

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->filepath, sizeof(cfg->filepath), "%s", filepath);

    if (!fs_exists(filepath)) return -1;

    char buf[4096];
    int n = fs_read_file(filepath, buf, sizeof(buf));
    if (n < 0) return -1;

    // Parse: key=value lines
    char *line = strtok(buf, "\n");
    while (line && cfg->count < CONFIG_MAX_ENTRIES) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0' || line[0] == '\n') {
            line = strtok(NULL, "\n");
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) {
            line = strtok(NULL, "\n");
            continue;
        }

        *eq = '\0';
        ConfigEntry *e = &cfg->entries[cfg->count++];
        snprintf(e->key, sizeof(e->key), "%s", line);
        snprintf(e->value, sizeof(e->value), "%s", eq + 1);

        line = strtok(NULL, "\n");
    }

    return cfg->count;
}

int config_save(const Config *cfg) {
    if (!cfg || cfg->filepath[0] == '\0') return -1;

    char buf[4096];
    int offset = 0;

    for (int i = 0; i < cfg->count; i++) {
        offset += snprintf(buf + offset, sizeof(buf) - offset,
                          "%s=%s\n", cfg->entries[i].key, cfg->entries[i].value);
    }

    return fs_write_file(cfg->filepath, buf, offset);
}

const char *config_get(const Config *cfg, const char *key) {
    if (!cfg || !key) return NULL;
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            return cfg->entries[i].value;
        }
    }
    return NULL;
}

int config_set(Config *cfg, const char *key, const char *value) {
    if (!cfg || !key || !value) return -1;

    // Update existing
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            snprintf(cfg->entries[i].value, sizeof(cfg->entries[i].value),
                     "%s", value);
            return 0;
        }
    }

    // Add new
    if (cfg->count >= CONFIG_MAX_ENTRIES) return -1;
    ConfigEntry *e = &cfg->entries[cfg->count++];
    snprintf(e->key, sizeof(e->key), "%s", key);
    snprintf(e->value, sizeof(e->value), "%s", value);
    return 0;
}

int config_delete(Config *cfg, const char *key) {
    if (!cfg || !key) return -1;
    for (int i = 0; i < cfg->count; i++) {
        if (strcmp(cfg->entries[i].key, key) == 0) {
            cfg->entries[i] = cfg->entries[cfg->count - 1];
            cfg->count--;
            return 0;
        }
    }
    return -1;
}
```

### 12.5 Tests

```c
// tests/test_config.c
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

// Funciones del mock FS
extern void mock_fs_reset(void);
extern void mock_fs_add_file(const char *path, const char *content);
extern int mock_fs_count(void);

static void setup(void) {
    mock_fs_reset();
}

static void test_load_basic(void) {
    setup();
    mock_fs_add_file("/etc/app.conf", "host=localhost\nport=8080\nlog=debug\n");

    Config cfg;
    int count = config_load(&cfg, "/etc/app.conf");
    assert(count == 3);
    assert(strcmp(config_get(&cfg, "host"), "localhost") == 0);
    assert(strcmp(config_get(&cfg, "port"), "8080") == 0);
    assert(strcmp(config_get(&cfg, "log"), "debug") == 0);

    printf("  PASS: load basic config\n");
}

static void test_load_with_comments(void) {
    setup();
    mock_fs_add_file("/etc/app.conf",
        "# This is a comment\n"
        "host=localhost\n"
        "# Another comment\n"
        "port=8080\n");

    Config cfg;
    int count = config_load(&cfg, "/etc/app.conf");
    assert(count == 2);
    assert(strcmp(config_get(&cfg, "host"), "localhost") == 0);

    printf("  PASS: load config with comments\n");
}

static void test_load_nonexistent_file(void) {
    setup();

    Config cfg;
    int rc = config_load(&cfg, "/etc/nonexistent.conf");
    assert(rc == -1);

    printf("  PASS: load nonexistent file\n");
}

static void test_get_nonexistent_key(void) {
    setup();
    mock_fs_add_file("/etc/app.conf", "host=localhost\n");

    Config cfg;
    config_load(&cfg, "/etc/app.conf");
    assert(config_get(&cfg, "nonexistent") == NULL);

    printf("  PASS: get nonexistent key\n");
}

static void test_set_new_key(void) {
    setup();
    mock_fs_add_file("/etc/app.conf", "host=localhost\n");

    Config cfg;
    config_load(&cfg, "/etc/app.conf");
    config_set(&cfg, "port", "9090");

    assert(cfg.count == 2);
    assert(strcmp(config_get(&cfg, "port"), "9090") == 0);

    printf("  PASS: set new key\n");
}

static void test_set_update_existing(void) {
    setup();
    mock_fs_add_file("/etc/app.conf", "host=localhost\n");

    Config cfg;
    config_load(&cfg, "/etc/app.conf");
    config_set(&cfg, "host", "0.0.0.0");

    assert(cfg.count == 1);
    assert(strcmp(config_get(&cfg, "host"), "0.0.0.0") == 0);

    printf("  PASS: set update existing key\n");
}

static void test_delete_key(void) {
    setup();
    mock_fs_add_file("/etc/app.conf", "host=localhost\nport=8080\n");

    Config cfg;
    config_load(&cfg, "/etc/app.conf");
    config_delete(&cfg, "host");

    assert(cfg.count == 1);
    assert(config_get(&cfg, "host") == NULL);
    assert(strcmp(config_get(&cfg, "port"), "8080") == 0);

    printf("  PASS: delete key\n");
}

static void test_save_and_reload(void) {
    setup();
    mock_fs_add_file("/etc/app.conf", "host=localhost\n");

    Config cfg;
    config_load(&cfg, "/etc/app.conf");
    config_set(&cfg, "port", "3000");
    config_set(&cfg, "log", "info");
    config_save(&cfg);

    // Reload from the mock FS (write + read roundtrip)
    Config cfg2;
    int count = config_load(&cfg2, "/etc/app.conf");
    assert(count == 3);
    assert(strcmp(config_get(&cfg2, "host"), "localhost") == 0);
    assert(strcmp(config_get(&cfg2, "port"), "3000") == 0);
    assert(strcmp(config_get(&cfg2, "log"), "info") == 0);

    printf("  PASS: save and reload\n");
}

int main(void) {
    printf("=== Config tests (mock filesystem, link-time substitution) ===\n");
    test_load_basic();
    test_load_with_comments();
    test_load_nonexistent_file();
    test_get_nonexistent_key();
    test_set_new_key();
    test_set_update_existing();
    test_delete_key();
    test_save_and_reload();
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

```bash
# Compilacion — config.c real + mock_filesystem en vez de filesystem real
gcc -I include -o test_config \
    tests/test_config.c \
    src/config.c \
    tests/mocks/mock_filesystem.c

./test_config
# === Config tests (mock filesystem, link-time substitution) ===
#   PASS: load basic config
#   PASS: load config with comments
#   PASS: load nonexistent file
#   PASS: get nonexistent key
#   PASS: set new key
#   PASS: set update existing key
#   PASS: delete key
#   PASS: save and reload
# === ALL TESTS PASSED ===
```

---

## 13. Errores comunes

### 13.1 Multiple definition

```bash
$ gcc test.o user_service.o database.o mock_database.o -o test
# /usr/bin/ld: mock_database.o: in function `db_find_user_by_email':
# mock_database.c:(.text+0x0): multiple definition of `db_find_user_by_email';
# database.o:(.text+0x0): first defined here

# CAUSA: ambos .o definen el mismo simbolo (strong)
# SOLUCION: linkear SOLO UNO de los dos
#   gcc test.o user_service.o mock_database.o -o test
# O usar weak symbols (seccion 8)
```

### 13.2 Undefined reference

```bash
$ gcc test.o user_service.o -o test
# /usr/bin/ld: user_service.o: undefined reference to `db_find_user_by_email'
# /usr/bin/ld: user_service.o: undefined reference to `db_insert_user'

# CAUSA: olvidaste linkear la implementacion (real O mock)
# SOLUCION: agregar el .o correspondiente
#   gcc test.o user_service.o mock_database.o -o test
```

### 13.3 Mock incompleto

```bash
$ gcc test.o user_service.o mock_database.o -o test
# /usr/bin/ld: user_service.o: undefined reference to `db_count_users'

# CAUSA: mock_database.c no implementa TODAS las funciones del header
# SOLUCION: implementar db_count_users en mock_database.c
# Aunque sea: int db_count_users(void) { return 0; }
```

### 13.4 Mock con firma diferente

```c
// database.h:
int db_find_user_by_email(const char *email, UserRecord *out);

// mock_database.c (BUG):
int db_find_user_by_email(char *email, UserRecord *out) {
    // const char* vs char* — diferente firma!
}

// El compilador puede advertir (con -Wall) pero el linker no verifica firmas.
// Resultado: undefined behavior (wrong arguments on the stack)

// SOLUCION: siempre #include el header en el mock para que el compilador verifique
#include "database.h"  // ← el compilador verifica la firma
```

### 13.5 Dependencia transitiva no mockeada

```c
// user_service.c llama a db_find_user_by_email
// database.c llama a pg_connect() de libpq

// Si linkeas con database.c (real), necesitas -lpq
// Si linkeas con mock_database.c, NO necesitas -lpq (no hay llamada a pg_connect)

// Error comun: linkear mock_database.o pero dejar -lpq en el Makefile
// No es error, pero es innecesario (y confuso)

// Error grave: tu mock llama a una funcion de otra dependencia no mockeada
// mock_database.c:
int db_find_user_by_email(const char *email, UserRecord *out) {
    audit_log("find_user called");  // ← si audit_log no esta linkeado, error!
    return -1;
}
// SOLUCION: los mocks deben ser autocontenidos (sin dependencias externas)
```

---

## 14. Programa de practica — gestor de logs con mocks de filesystem y clock

### 14.1 Requisitos

Implementa un modulo de logging que:

```c
// log_manager.h
typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } LogLevel;

typedef struct LogManager LogManager;

LogManager *log_manager_new(const char *base_dir, LogLevel min_level);
void log_manager_free(LogManager *lm);

// Escribe un log. Crea archivo con nombre basado en la fecha: YYYY-MM-DD.log
int log_write(LogManager *lm, LogLevel level, const char *message);

// Rotacion: eliminar archivos de log con mas de max_days dias de antiguedad
int log_rotate(LogManager *lm, int max_days);

// Contar lineas totales en el log de hoy
int log_count_today(LogManager *lm);
```

**Dependencias** (a mockear):
- `filesystem.h` — leer/escribir/listar/eliminar archivos
- `clock.h` — obtener la hora actual (para nombre de archivo y rotacion)

### 14.2 Lo que debes implementar

1. `log_manager.c` — usa `filesystem.h` y `clock.h` sin saber si son reales o mocks
2. `tests/mocks/mock_filesystem.c` — fake filesystem en memoria (reutilizar el de seccion 12)
3. `tests/mocks/mock_clock.c` — stub de time con setter
4. `tests/test_log_manager.c` — 12+ tests:
   - Escribir un log crea el archivo correcto (fecha en nombre)
   - Cada log agrega una linea al archivo
   - El nivel minimo filtra logs de menor prioridad
   - log_count_today retorna el conteo correcto
   - log_rotate elimina archivos de mas de N dias
   - log_rotate no elimina archivos recientes
   - Cambiar la fecha del stub clock cambia el nombre del archivo

5. **Makefile** con dos targets:
   ```makefile
   # Test: mock filesystem + mock clock
   test_log_manager: tests/test_log_manager.o src/log_manager.o \
                     tests/mocks/mock_filesystem.o tests/mocks/mock_clock.o

   # Produccion: real filesystem + real clock
   app: src/main.o src/log_manager.o src/filesystem.o src/clock.o
   ```

---

## 15. Ejercicios

### Ejercicio 1: Mock de la standard library

`time()` es parte de la standard library. No puedes simplemente crear otro `.c` con `time_t time(time_t *t)` porque conflicta con libc. Implementa tres soluciones diferentes:

1. **Wrapper**: crear `my_time()` que llama a `time()`, y el codigo usa `my_time()`. Mock de `my_time()` via link-time substitution.
2. **Weak symbol**: marcar `my_time()` como weak en produccion, strong en el mock.
3. **--wrap**: usar `-Wl,--wrap=time` para interceptar la llamada real (preview del proximo topico).

Compara las tres soluciones: lineas de codigo, invasividad, portabilidad.

### Ejercicio 2: Spy con link-time substitution

Crea un spy de `filesystem.h` via link-time substitution que:
- Grabe TODAS las operaciones (que funcion, que path, que contenido)
- Permita reproducir la secuencia de operaciones (replay)
- Tenga una funcion `spy_fs_dump()` que imprima todas las operaciones grabadas

Usa el spy para verificar que `config_save` escribe exactamente el contenido esperado, con las claves en el orden correcto.

### Ejercicio 3: Weak symbols parciales

Usando weak symbols, crea un test donde:
- `fs_read_file` y `fs_write_file` usan la implementacion REAL (disco real)
- `fs_delete` y `fs_list_dir` usan mocks (para no borrar archivos reales)

Escribe un test que lea un archivo real del disco, pero no lo borre cuando `config_delete` llama a `fs_delete`. Verifica que el test es seguro (no destruye datos).

### Ejercicio 4: CMake con seleccion de mock

Crea un `CMakeLists.txt` que permita elegir la implementacion via opcion:

```bash
cmake -B build -DUSE_MOCK_DATABASE=ON -DUSE_MOCK_EMAIL=OFF
```

Cada dependencia puede ser real o mock independientemente. Usa `option()` y `if()` de CMake. Verifica que `ctest` funciona con todas las combinaciones.

---

Navegacion:

- Anterior: [T02 — Function pointer injection](../T02_Function_pointer_injection/README.md)
- Siguiente: [T04 — CMocka mocking](../T04_CMocka_mocking/README.md)
- Indice: [BLOQUE 17 — Testing e Ingenieria de Software](../../../BLOQUE_17.md)

---

*Proximo topico: T04 — CMocka mocking*
