# CMocka mocking — wrapping functions con --wrap, will_return, mock(), expect_function_call

## 1. Introduccion

En los topicos anteriores vimos tres mecanismos de inyeccion: function pointer injection (T02) y link-time substitution (T03) requieren que TU controles la compilacion y, en el primer caso, modifiques la API. El tercer mecanismo — **linker wrapping con `--wrap`** — permite interceptar llamadas a CUALQUIER funcion (incluyendo funciones de la standard library) sin modificar ni una linea del codigo de produccion. CMocka esta diseñado para explotar este mecanismo al maximo.

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                     --wrap: EL MECANISMO                                        │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  gcc -Wl,--wrap=send_email test.c user_service.c -lcmocka -o test               │
│                                                                                  │
│  ¿Que hace --wrap=send_email?                                                   │
│                                                                                  │
│  1. TODA llamada a send_email() en el programa se redirige a                    │
│     __wrap_send_email()                                                          │
│                                                                                  │
│  2. La funcion original queda accesible como __real_send_email()                │
│                                                                                  │
│  ┌──────────────────────┐                                                       │
│  │  user_service.c      │                                                       │
│  │                      │     ANTES (sin --wrap):                               │
│  │  send_email(to, ...) │────▶ send_email() en email.o                          │
│  │                      │                                                       │
│  │                      │     CON --wrap:                                        │
│  │  send_email(to, ...) │────▶ __wrap_send_email() en test.o                    │
│  │                      │                                                       │
│  │                      │     Dentro de __wrap_send_email:                       │
│  │                      │     puedes llamar __real_send_email() si quieres      │
│  │                      │     ejecutar tambien la funcion original              │
│  └──────────────────────┘                                                       │
│                                                                                  │
│  Ventajas sobre link-time substitution:                                         │
│  • Puedes wrappear funciones de la standard library (malloc, time, fopen)       │
│  • Puedes wrappear funciones de librerias de terceros                           │
│  • Puedes llamar a la funcion REAL desde el wrapper (__real_*)                  │
│  • No necesitas compilar un .o separado para cada mock                          │
│  • El mock esta JUNTO al test (un solo archivo)                                 │
│                                                                                  │
│  Limitaciones:                                                                  │
│  • Solo funciona con GNU ld (Linux). No funciona en macOS ni Windows.           │
│  • En macOS se puede lograr algo similar con -interposing o DYLD_INTERPOSE      │
│  • Las llamadas internas dentro del mismo .o NO se wrappean                     │
│    (el linker solo intercepta llamadas entre translation units)                 │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1.1 --wrap sin CMocka (puro C)

Para entender el mecanismo antes de agregar CMocka:

```c
// email.h
int send_email(const char *to, const char *subject, const char *body);

// email.c (implementacion real)
#include "email.h"
#include <stdio.h>

int send_email(const char *to, const char *subject, const char *body) {
    printf("REAL: Sending email to %s: %s\n", to, subject);
    // ... conectar a SMTP, enviar ...
    return 0;
}
```

```c
// test_with_wrap.c
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "email.h"

// Declarar la funcion real (disponible como __real_*)
extern int __real_send_email(const char *to, const char *subject,
                              const char *body);

// Definir el wrapper
int __wrap_send_email(const char *to, const char *subject, const char *body) {
    printf("WRAPPED: Intercepted email to %s\n", to);

    // Podemos verificar argumentos
    assert(to != NULL);
    assert(subject != NULL);

    // Podemos decidir si llamar a la real o no:
    // return __real_send_email(to, subject, body);  // delegar a la real
    return 0;  // o simplemente retornar sin hacer nada
}

// Funcion que usa send_email (codigo de produccion)
extern int notify_user(const char *email, const char *msg);

int main(void) {
    // Cuando notify_user() llama a send_email(),
    // en realidad ejecuta __wrap_send_email()
    notify_user("alice@test.com", "Hello");
    printf("PASS\n");
    return 0;
}
```

```bash
# Compilar con --wrap
gcc -o test_with_wrap test_with_wrap.c user_service.c email.c \
    -Wl,--wrap=send_email

# Ejecutar
./test_with_wrap
# WRAPPED: Intercepted email to alice@test.com
# PASS

# Sin --wrap, send_email iria a la implementacion real de email.c
```

---

## 2. CMocka + --wrap — la combinacion

CMocka proporciona macros que hacen el wrapping mucho mas poderoso. En vez de verificar argumentos con assert() manual, usas `check_expected()` (mock verifica automaticamente). En vez de retornar valores hardcodeados, usas `mock_type()` (configurable por test).

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  CMOCKA + --wrap = MOCKING COMPLETO                                             │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  Sin CMocka (--wrap puro):                                                      │
│  ┌──────────────────────────────────────────────────────────┐                    │
│  │  int __wrap_send_email(const char *to, ...) {            │                    │
│  │      assert(strcmp(to, "alice@test.com") == 0);  // manual│                    │
│  │      return 0;                                  // fijo  │                    │
│  │  }                                                       │                    │
│  └──────────────────────────────────────────────────────────┘                    │
│  Problema: el wrapper es especifico para UN test. Si otro test                  │
│  quiere que retorne -1 o espera otro email, necesitas otro wrapper.             │
│                                                                                  │
│  Con CMocka:                                                                    │
│  ┌──────────────────────────────────────────────────────────┐                    │
│  │  int __wrap_send_email(const char *to, ...) {            │                    │
│  │      check_expected(to);           // CMocka verifica    │                    │
│  │      check_expected(subject);      // CMocka verifica    │                    │
│  │      return mock_type(int);        // CMocka provee      │                    │
│  │  }                                                       │                    │
│  │                                                          │                    │
│  │  // Test 1: espera alice, retorna 0                      │                    │
│  │  expect_string(__wrap_send_email, to, "alice@test.com"); │                    │
│  │  will_return(__wrap_send_email, 0);                      │                    │
│  │                                                          │                    │
│  │  // Test 2: espera bob, retorna -1                       │                    │
│  │  expect_string(__wrap_send_email, to, "bob@test.com");   │                    │
│  │  will_return(__wrap_send_email, -1);                      │                    │
│  └──────────────────────────────────────────────────────────┘                    │
│  UN SOLO wrapper sirve para TODOS los tests. Cada test configura               │
│  sus propias expectativas con expect_* y will_return.                           │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. API de mocking de CMocka — referencia completa

### 3.1 will_return — configurar valores de retorno

```c
// will_return(function, value)
// Encola un valor que mock_type() retornara la proxima vez
// que se llame a `function`.

// --- Un solo valor ---
will_return(__wrap_db_insert, 42);
// La proxima llamada a __wrap_db_insert que haga mock_type(int)
// obtendra 42.

// --- Multiples valores (cola FIFO) ---
will_return(__wrap_db_insert, 1);   // primera llamada → 1
will_return(__wrap_db_insert, 2);   // segunda llamada → 2
will_return(__wrap_db_insert, -1);  // tercera llamada → -1
// Si se llama una cuarta vez sin will_return, CMocka falla el test.

// --- Repetir el mismo valor N veces ---
will_return_count(__wrap_db_insert, 0, 5);
// Las proximas 5 llamadas retornaran 0.

// --- Repetir siempre ---
will_return_always(__wrap_db_insert, 0);
// TODAS las llamadas retornaran 0 (sin limite).
// Util cuando no sabes cuantas veces se llamara.
```

### 3.2 mock_type — obtener el valor en el wrapper

```c
// Dentro del __wrap_*, usa mock_type(tipo) para obtener el valor:

int __wrap_db_insert(const UserRecord *user) {
    // check_expected(...)  — verificar parametros (seccion 3.3)
    return mock_type(int);  // retorna el valor de will_return
}

// mock_type cast al tipo especificado:
int __wrap_read_sensor(int sensor_id) {
    check_expected(sensor_id);
    return mock_type(int);
}

// Para punteros:
const char *__wrap_get_config(const char *key) {
    check_expected(key);
    return mock_ptr_type(const char *);
    // mock_ptr_type para punteros (evita warnings de cast int↔pointer)
}

// En el test:
will_return(__wrap_get_config, "localhost");  // retornara "localhost"
// o para NULL:
will_return(__wrap_get_config, NULL);
```

### 3.3 expect_* — definir expectativas sobre parametros

```c
// expect_* define QUE VALOR debe tener un parametro cuando el wrapper
// es llamado. check_expected() dentro del wrapper lo verifica.

// === TIPOS BASICOS ===

// Verificar valor exacto
expect_value(__wrap_func, param_name, expected_value);
// Ejemplo:
expect_value(__wrap_db_delete, user_id, 42);
// Si __wrap_db_delete es llamado con user_id != 42, CMocka falla el test.

// Verificar string
expect_string(__wrap_func, param_name, expected_string);
// Ejemplo:
expect_string(__wrap_send_email, to, "alice@test.com");
// Usa strcmp internamente. Si to != "alice@test.com", falla.

// Verificar memoria (N bytes)
expect_memory(__wrap_func, param_name, expected_data, size);
// Ejemplo:
char expected_hash[32] = {...};
expect_memory(__wrap_db_insert, password_hash, expected_hash, 32);

// Verificar que el parametro NO es NULL
expect_not_value(__wrap_func, param_name, NULL);

// Verificar rango
expect_in_range(__wrap_func, param_name, min, max);
// Ejemplo:
expect_in_range(__wrap_set_temperature, value, 0, 100);

expect_not_in_range(__wrap_func, param_name, min, max);

// === NO VERIFICAR (aceptar cualquier valor) ===
expect_any(__wrap_func, param_name);
// No verifica el valor, pero registra que el parametro existe.
// Util cuando solo quieres verificar ALGUNOS parametros.

// === REPETICION ===
expect_value_count(__wrap_func, param_name, value, count);
// Las proximas `count` llamadas deben tener param_name == value

expect_any_count(__wrap_func, param_name, count);
// Las proximas `count` llamadas: no verificar param_name

// Siempre (sin limite):
expect_any_always(__wrap_func, param_name);
expect_value_always(__wrap_func, param_name, value);
expect_string_always(__wrap_func, param_name, string);
```

### 3.4 check_expected — verificar en el wrapper

```c
// Dentro del __wrap_*, check_expected verifica que el parametro
// cumple la expectativa definida con expect_*.

int __wrap_send_email(const char *to, const char *subject, const char *body) {
    check_expected(to);        // verifica contra expect_string/expect_value/etc.
    check_expected(subject);   // idem
    check_expected(body);      // idem
    return mock_type(int);
}

// Si el test definio:
//   expect_string(__wrap_send_email, to, "alice@test.com");
// Y el codigo llama send_email("bob@test.com", ...):
// → CMocka imprime:
//   Error: Expected parameter 'to' to be string "alice@test.com"
//   but got "bob@test.com"
// Y falla el test.

// check_expected_ptr — para punteros (evita warnings):
void __wrap_process(const void *data, int size) {
    check_expected_ptr(data);
    check_expected(size);
}
```

### 3.5 expect_function_call / function_called

```c
// Verificar que una funcion FUE llamada (sin verificar parametros)

// En el test:
expect_function_call(__wrap_send_email);
// → CMocka falla si __wrap_send_email nunca se ejecuta

// En el wrapper:
int __wrap_send_email(const char *to, const char *subject, const char *body) {
    function_called();  // registra que esta funcion fue llamada
    return mock_type(int);
}

// Combinar con expect_*:
expect_function_call(__wrap_send_email);
expect_string(__wrap_send_email, to, "alice@test.com");
will_return(__wrap_send_email, 0);

// Multiples llamadas esperadas:
expect_function_calls(__wrap_send_email, 3);
// Espera que __wrap_send_email sea llamada exactamente 3 veces

// NO esperar llamada:
// Si no pones expect_function_call ni expect_*, y el wrapper
// llama a check_expected o function_called, CMocka falla porque
// no hay expectativa definida.
// Esto es "mock estricto": llamadas inesperadas son errores.
```

### 3.6 Tabla resumen de la API

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  CMOCKA MOCKING API — RESUMEN                                                  │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  EN EL TEST (configurar):                                                       │
│  ┌────────────────────────────────────────────────────────────────────┐          │
│  │  will_return(fn, val)              Encolar valor de retorno       │          │
│  │  will_return_count(fn, val, n)     Encolar val para n llamadas    │          │
│  │  will_return_always(fn, val)       val para todas las llamadas    │          │
│  │                                                                    │          │
│  │  expect_value(fn, param, val)      Esperar param == val           │          │
│  │  expect_string(fn, param, str)     Esperar param == str           │          │
│  │  expect_memory(fn, param, mem, n)  Esperar param == mem (n bytes) │          │
│  │  expect_in_range(fn, param, lo,hi) Esperar lo <= param <= hi      │          │
│  │  expect_not_value(fn, param, val)  Esperar param != val           │          │
│  │  expect_any(fn, param)             Aceptar cualquier valor        │          │
│  │                                                                    │          │
│  │  expect_function_call(fn)          Esperar que fn sea llamada     │          │
│  │  expect_function_calls(fn, n)      Esperar n llamadas a fn        │          │
│  │                                                                    │          │
│  │  Variantes _count y _always disponibles para todos los expect_*   │          │
│  └────────────────────────────────────────────────────────────────────┘          │
│                                                                                  │
│  EN EL WRAPPER (verificar):                                                     │
│  ┌────────────────────────────────────────────────────────────────────┐          │
│  │  check_expected(param)             Verificar parametro            │          │
│  │  check_expected_ptr(param)         Verificar puntero              │          │
│  │  mock_type(type)                   Obtener valor de retorno       │          │
│  │  mock_ptr_type(type)               Obtener puntero de retorno     │          │
│  │  function_called()                 Registrar que fn fue llamada   │          │
│  └────────────────────────────────────────────────────────────────────┘          │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Ejemplo completo — servicio de usuario con mocking

### 4.1 Codigo de produccion

```c
// include/database.h
#ifndef DATABASE_H
#define DATABASE_H

typedef struct {
    int id;
    char name[64];
    char email[128];
    char password_hash[128];
    int active;
} UserRecord;

int db_find_user_by_email(const char *email, UserRecord *out);
int db_insert_user(const UserRecord *user);
int db_delete_user(int user_id);
int db_update_user(const UserRecord *user);

#endif
```

```c
// include/email.h
#ifndef EMAIL_H
#define EMAIL_H

int send_email(const char *to, const char *subject, const char *body);

#endif
```

```c
// include/hasher.h
#ifndef HASHER_H
#define HASHER_H

int hash_password(const char *password, char *hash, int hash_size);

#endif
```

```c
// include/user_service.h
#ifndef USER_SERVICE_H
#define USER_SERVICE_H

typedef enum {
    US_OK = 0,
    US_ERR_INVALID = -1,
    US_ERR_DUPLICATE = -2,
    US_ERR_DB = -3,
    US_ERR_NOT_FOUND = -4,
    US_ERR_HASH = -5,
} UserServiceResult;

int user_service_register(const char *name, const char *email,
                          const char *password);
int user_service_deactivate(int user_id);
int user_service_change_email(int user_id, const char *new_email);

#endif
```

```c
// src/user_service.c — codigo de produccion (CERO referencias a mocks)
#include "user_service.h"
#include "database.h"
#include "email.h"
#include "hasher.h"
#include <string.h>
#include <stdio.h>

static int validate_email(const char *email) {
    if (!email) return 0;
    const char *at = strchr(email, '@');
    if (!at || at == email) return 0;
    const char *dot = strrchr(at, '.');
    if (!dot || dot == at + 1 || strlen(dot + 1) < 2) return 0;
    return 1;
}

int user_service_register(const char *name, const char *email,
                          const char *password) {
    // Validar inputs
    if (!name || strlen(name) == 0) return US_ERR_INVALID;
    if (!validate_email(email)) return US_ERR_INVALID;
    if (!password || strlen(password) < 8) return US_ERR_INVALID;

    // Verificar que no exista
    UserRecord existing;
    if (db_find_user_by_email(email, &existing) == 0) {
        return US_ERR_DUPLICATE;
    }

    // Hashear password
    UserRecord new_user = {0};
    snprintf(new_user.name, sizeof(new_user.name), "%s", name);
    snprintf(new_user.email, sizeof(new_user.email), "%s", email);
    new_user.active = 1;

    if (hash_password(password, new_user.password_hash,
                      sizeof(new_user.password_hash)) != 0) {
        return US_ERR_HASH;
    }

    // Insertar en DB
    int id = db_insert_user(&new_user);
    if (id < 0) return US_ERR_DB;

    // Enviar email de bienvenida (best effort — no falla si email falla)
    char body[256];
    snprintf(body, sizeof(body),
             "Welcome %s!\nYour account has been created.", name);
    send_email(email, "Welcome!", body);

    return id;
}

int user_service_deactivate(int user_id) {
    UserRecord user;
    // Buscar por ID iterando (simplificacion)
    // En produccion habria db_find_by_id
    // Aqui asumimos que tenemos la funcion
    // Para este ejemplo, usamos update directo

    // Buscar usuario actual
    user.id = user_id;
    user.active = 0;

    int rc = db_update_user(&user);
    if (rc != 0) return US_ERR_NOT_FOUND;

    return US_OK;
}

int user_service_change_email(int user_id, const char *new_email) {
    if (!validate_email(new_email)) return US_ERR_INVALID;

    // Verificar que el nuevo email no este en uso
    UserRecord existing;
    if (db_find_user_by_email(new_email, &existing) == 0) {
        return US_ERR_DUPLICATE;
    }

    // Actualizar
    UserRecord update = {.id = user_id};
    snprintf(update.email, sizeof(update.email), "%s", new_email);
    update.active = 1;

    int rc = db_update_user(&update);
    if (rc != 0) return US_ERR_DB;

    // Notificar al nuevo email
    send_email(new_email, "Email Updated",
               "Your email has been updated successfully.");

    return US_OK;
}
```

### 4.2 Wrappers (mocks CMocka)

```c
// tests/test_user_service.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "user_service.h"
#include "database.h"

// =====================================================================
// WRAPPERS — definen como se verifican y que retornan las dependencias
// =====================================================================

// --- Wrapper de db_find_user_by_email ---
int __wrap_db_find_user_by_email(const char *email, UserRecord *out) {
    check_expected(email);

    int rc = mock_type(int);

    // Si se configuro un record para copiar:
    if (rc == 0 && out) {
        UserRecord *mock_record = mock_ptr_type(UserRecord *);
        if (mock_record) {
            *out = *mock_record;
        }
    }

    return rc;
}

// --- Wrapper de db_insert_user ---
int __wrap_db_insert_user(const UserRecord *user) {
    check_expected(user);  // verifica que el puntero no sea NULL (o lo que pidas)
    // Podemos inspeccionar los campos del user:
    check_expected(user->email);
    return mock_type(int);
}

// --- Wrapper de db_delete_user ---
int __wrap_db_delete_user(int user_id) {
    check_expected(user_id);
    return mock_type(int);
}

// --- Wrapper de db_update_user ---
int __wrap_db_update_user(const UserRecord *user) {
    check_expected(user->id);
    return mock_type(int);
}

// --- Wrapper de send_email ---
int __wrap_send_email(const char *to, const char *subject, const char *body) {
    check_expected(to);
    check_expected(subject);
    // No verificamos body en la mayoria de tests (muy fragil)
    return mock_type(int);
}

// --- Wrapper de hash_password ---
int __wrap_hash_password(const char *password, char *hash, int hash_size) {
    check_expected(password);
    // Escribir un hash fake en el buffer
    const char *fake_hash = mock_ptr_type(const char *);
    if (fake_hash && hash && hash_size > 0) {
        snprintf(hash, hash_size, "%s", fake_hash);
    }
    return mock_type(int);
}

// =====================================================================
// HELPERS — reducir repeticion en los tests
// =====================================================================

// Helper: configurar db_find para "no encontrado"
static void expect_find_not_found(const char *email) {
    expect_string(__wrap_db_find_user_by_email, email, email);
    will_return(__wrap_db_find_user_by_email, -1);  // no encontrado
}

// Helper: configurar db_find para "encontrado"
static void expect_find_found(const char *email, UserRecord *record) {
    expect_string(__wrap_db_find_user_by_email, email, email);
    will_return(__wrap_db_find_user_by_email, 0);         // encontrado
    will_return(__wrap_db_find_user_by_email, record);    // puntero al record
}

// Helper: configurar hash_password exitoso
static void expect_hash_ok(const char *password) {
    expect_string(__wrap_hash_password, password, password);
    will_return(__wrap_hash_password, "hashed_value");  // fake hash
    will_return(__wrap_hash_password, 0);               // success
}

// Helper: configurar db_insert exitoso
static void expect_insert_ok(const char *email, int returned_id) {
    expect_any(__wrap_db_insert_user, user);
    expect_string(__wrap_db_insert_user, user->email, email);
    will_return(__wrap_db_insert_user, returned_id);
}

// Helper: configurar send_email (no verificar contenido)
static void expect_email_sent(const char *to) {
    expect_string(__wrap_send_email, to, to);
    expect_any(__wrap_send_email, subject);
    will_return(__wrap_send_email, 0);
}

// =====================================================================
// TESTS
// =====================================================================

// --- Registro exitoso ---
static void test_register_success(void **state) {
    (void)state;

    // Configurar expectativas en ORDEN de ejecucion
    expect_find_not_found("alice@test.com");
    expect_hash_ok("password123");
    expect_insert_ok("alice@test.com", 42);
    expect_email_sent("alice@test.com");

    // Ejecutar
    int id = user_service_register("Alice", "alice@test.com", "password123");

    // Verificar resultado
    assert_int_equal(id, 42);
}

// --- Registro con email duplicado ---
static void test_register_duplicate_email(void **state) {
    (void)state;

    UserRecord existing = {.id = 1, .active = 1};
    snprintf(existing.email, sizeof(existing.email), "alice@test.com");
    snprintf(existing.name, sizeof(existing.name), "Existing Alice");

    expect_find_found("alice@test.com", &existing);
    // No esperamos hash, insert, ni email (el codigo retorna antes)

    int rc = user_service_register("Alice", "alice@test.com", "password123");
    assert_int_equal(rc, US_ERR_DUPLICATE);
}

// --- Registro con input invalido (no llega a la DB) ---
static void test_register_invalid_email(void **state) {
    (void)state;
    // No configuramos expectativas porque las funciones mockeadas
    // NO deben ser llamadas

    int rc = user_service_register("Alice", "not-an-email", "password123");
    assert_int_equal(rc, US_ERR_INVALID);
}

static void test_register_short_password(void **state) {
    (void)state;

    int rc = user_service_register("Alice", "alice@test.com", "short");
    assert_int_equal(rc, US_ERR_INVALID);
}

static void test_register_null_name(void **state) {
    (void)state;

    int rc = user_service_register(NULL, "alice@test.com", "password123");
    assert_int_equal(rc, US_ERR_INVALID);
}

// --- Registro con fallo de DB ---
static void test_register_db_insert_fails(void **state) {
    (void)state;

    expect_find_not_found("alice@test.com");
    expect_hash_ok("password123");
    // Insert falla
    expect_any(__wrap_db_insert_user, user);
    expect_any(__wrap_db_insert_user, user->email);
    will_return(__wrap_db_insert_user, -1);  // error
    // No esperamos email (el codigo retorna antes de enviar)

    int rc = user_service_register("Alice", "alice@test.com", "password123");
    assert_int_equal(rc, US_ERR_DB);
}

// --- Registro con fallo de hash ---
static void test_register_hash_fails(void **state) {
    (void)state;

    expect_find_not_found("alice@test.com");
    // Hash falla
    expect_string(__wrap_hash_password, password, "password123");
    will_return(__wrap_hash_password, NULL);  // no hay hash
    will_return(__wrap_hash_password, -1);    // error

    int rc = user_service_register("Alice", "alice@test.com", "password123");
    assert_int_equal(rc, US_ERR_HASH);
}

// --- Registro: email de bienvenida falla pero no bloquea ---
static void test_register_email_failure_does_not_block(void **state) {
    (void)state;

    expect_find_not_found("alice@test.com");
    expect_hash_ok("password123");
    expect_insert_ok("alice@test.com", 42);

    // Email falla
    expect_string(__wrap_send_email, to, "alice@test.com");
    expect_any(__wrap_send_email, subject);
    will_return(__wrap_send_email, -1);  // fallo de email

    // Pero register debe retornar exito de todas formas
    int id = user_service_register("Alice", "alice@test.com", "password123");
    assert_int_equal(id, 42);
}

// --- Deactivacion ---
static void test_deactivate_success(void **state) {
    (void)state;

    expect_value(__wrap_db_update_user, user->id, 5);
    will_return(__wrap_db_update_user, 0);  // success

    int rc = user_service_deactivate(5);
    assert_int_equal(rc, US_OK);
}

static void test_deactivate_not_found(void **state) {
    (void)state;

    expect_value(__wrap_db_update_user, user->id, 999);
    will_return(__wrap_db_update_user, -1);  // not found

    int rc = user_service_deactivate(999);
    assert_int_equal(rc, US_ERR_NOT_FOUND);
}

// --- Cambio de email ---
static void test_change_email_success(void **state) {
    (void)state;

    expect_find_not_found("newalice@test.com");

    expect_value(__wrap_db_update_user, user->id, 1);
    will_return(__wrap_db_update_user, 0);

    expect_string(__wrap_send_email, to, "newalice@test.com");
    expect_any(__wrap_send_email, subject);
    will_return(__wrap_send_email, 0);

    int rc = user_service_change_email(1, "newalice@test.com");
    assert_int_equal(rc, US_OK);
}

static void test_change_email_already_in_use(void **state) {
    (void)state;

    UserRecord existing = {.id = 2};
    snprintf(existing.email, sizeof(existing.email), "taken@test.com");
    expect_find_found("taken@test.com", &existing);

    int rc = user_service_change_email(1, "taken@test.com");
    assert_int_equal(rc, US_ERR_DUPLICATE);
}

static void test_change_email_invalid(void **state) {
    (void)state;

    int rc = user_service_change_email(1, "not-valid");
    assert_int_equal(rc, US_ERR_INVALID);
}

// =====================================================================
// MAIN — registrar y ejecutar tests
// =====================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_register_success),
        cmocka_unit_test(test_register_duplicate_email),
        cmocka_unit_test(test_register_invalid_email),
        cmocka_unit_test(test_register_short_password),
        cmocka_unit_test(test_register_null_name),
        cmocka_unit_test(test_register_db_insert_fails),
        cmocka_unit_test(test_register_hash_fails),
        cmocka_unit_test(test_register_email_failure_does_not_block),
        cmocka_unit_test(test_deactivate_success),
        cmocka_unit_test(test_deactivate_not_found),
        cmocka_unit_test(test_change_email_success),
        cmocka_unit_test(test_change_email_already_in_use),
        cmocka_unit_test(test_change_email_invalid),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 4.3 Compilacion

```bash
gcc -o test_user_service \
    tests/test_user_service.c \
    src/user_service.c \
    -I include \
    -lcmocka \
    -Wl,--wrap=db_find_user_by_email \
    -Wl,--wrap=db_insert_user \
    -Wl,--wrap=db_delete_user \
    -Wl,--wrap=db_update_user \
    -Wl,--wrap=send_email \
    -Wl,--wrap=hash_password

./test_user_service
# [==========] Running 13 test(s).
# [ RUN      ] test_register_success
# [       OK ] test_register_success
# [ RUN      ] test_register_duplicate_email
# [       OK ] test_register_duplicate_email
# ...
# [==========] 13 test(s) run.
# [  PASSED  ] 13 test(s).
```

### 4.4 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(cmocka_mocking_example C)
set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG        cmocka-1.1.7
)
set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmocka)

# Libreria bajo test
add_library(user_service src/user_service.c)
target_include_directories(user_service PUBLIC include)

# Tests
enable_testing()

add_executable(test_user_service tests/test_user_service.c)
target_link_libraries(test_user_service PRIVATE user_service cmocka-static)
target_include_directories(test_user_service PRIVATE include)

# --wrap flags
target_link_options(test_user_service PRIVATE
    -Wl,--wrap=db_find_user_by_email
    -Wl,--wrap=db_insert_user
    -Wl,--wrap=db_delete_user
    -Wl,--wrap=db_update_user
    -Wl,--wrap=send_email
    -Wl,--wrap=hash_password
)

add_test(NAME test_user_service COMMAND test_user_service)
set_tests_properties(test_user_service PROPERTIES
    LABELS "unit;mock"
    TIMEOUT 10
)
```

---

## 5. Wrapping funciones de la standard library

### 5.1 Wrapping malloc/free — detectar leaks y fallos

```c
// Wrapper de malloc para simular out-of-memory
void *__wrap_malloc(size_t size) {
    // Si el test configuro un fallo:
    if (mock_type(int) == 0) {
        return NULL;  // simular OOM
    }
    // Delegar a la real:
    return __real_malloc(size);
}

// En el test:
static void test_handles_out_of_memory(void **state) {
    (void)state;

    will_return(__wrap_malloc, 0);  // fallar

    void *ptr = create_something();  // internamente llama malloc
    assert_null(ptr);  // debe manejar el fallo gracefully
}

static void test_normal_allocation(void **state) {
    (void)state;

    will_return(__wrap_malloc, 1);  // permitir

    void *ptr = create_something();
    assert_non_null(ptr);
    // ...
    free(ptr);
}
```

```bash
gcc -o test_oom test_oom.c mylib.c -lcmocka -Wl,--wrap=malloc
```

### 5.2 Wrapping time()

```c
// Wrapper de time() — controlar el reloj
time_t __wrap_time(time_t *t) {
    time_t fake_time = mock_type(time_t);
    if (t) *t = fake_time;
    return fake_time;
}

// En el test:
static void test_token_expires_after_one_hour(void **state) {
    (void)state;

    // Primer time(): creacion del token
    will_return(__wrap_time, 1000);
    Token *tok = token_create("user1", 3600);

    // Segundo time(): verificacion (dentro del TTL)
    will_return(__wrap_time, 4500);  // 3500 segundos despues
    assert_true(token_is_valid(tok));

    // Tercer time(): verificacion (fuera del TTL)
    will_return(__wrap_time, 5000);  // 4000 segundos despues
    assert_false(token_is_valid(tok));

    token_free(tok);
}
```

### 5.3 Wrapping fopen/fread/fclose

```c
// Wrapper de fopen — simular archivos sin tocar el disco
FILE *__wrap_fopen(const char *path, const char *mode) {
    check_expected(path);
    check_expected(mode);

    // Retornar un FILE* fake o NULL
    return mock_ptr_type(FILE *);
}

char *__wrap_fgets(char *buf, int n, FILE *stream) {
    (void)stream;
    const char *mock_line = mock_ptr_type(const char *);
    if (!mock_line) return NULL;
    snprintf(buf, n, "%s", mock_line);
    return buf;
}

int __wrap_fclose(FILE *stream) {
    (void)stream;
    return mock_type(int);
}

// En el test:
static void test_read_config_file(void **state) {
    (void)state;

    // Simular que fopen tiene exito
    expect_string(__wrap_fopen, path, "/etc/myapp.conf");
    expect_string(__wrap_fopen, mode, "r");
    will_return(__wrap_fopen, (FILE *)0x1234);  // puntero fake no-NULL

    // Simular contenido del archivo
    will_return(__wrap_fgets, "host=localhost");
    will_return(__wrap_fgets, "port=8080");
    will_return(__wrap_fgets, NULL);  // EOF

    // Simular cierre exitoso
    will_return(__wrap_fclose, 0);

    Config cfg = read_config("/etc/myapp.conf");
    assert_string_equal(config_get(&cfg, "host"), "localhost");
    assert_string_equal(config_get(&cfg, "port"), "8080");
}
```

```bash
gcc -o test_config test_config.c config.c -lcmocka \
    -Wl,--wrap=fopen -Wl,--wrap=fgets -Wl,--wrap=fclose
```

---

## 6. Patron: __real_* para delegacion condicional

El prefijo `__real_` permite llamar a la funcion original desde el wrapper:

```c
// Wrapper que delega al real excepto en ciertos casos
void *__wrap_malloc(size_t size) {
    // Verificar si debemos fallar
    int should_fail = mock_type(int);
    if (should_fail) {
        return NULL;
    }
    // Delegar a la implementacion real
    return __real_malloc(size);
}

// Wrapper que agrega logging alrededor de la real
int __wrap_db_insert_user(const UserRecord *user) {
    printf("[SPY] db_insert_user called with email=%s\n", user->email);
    int result = __real_db_insert_user(user);  // llamar a la real
    printf("[SPY] db_insert_user returned %d\n", result);
    return result;
}

// Wrapper que verifica y delega
int __wrap_send_email(const char *to, const char *subject, const char *body) {
    check_expected(to);
    check_expected(subject);

    int use_real = mock_type(int);
    if (use_real) {
        return __real_send_email(to, subject, body);
    }
    return mock_type(int);
}
```

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  __real_ — USOS                                                                │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  1. PARTIAL MOCK: mockear solo bajo ciertas condiciones                         │
│     if (should_mock) return mock_value; else return __real_func();              │
│                                                                                  │
│  2. SPY PASSTHROUGH: grabar y pasar al real                                     │
│     log(args); result = __real_func(args); log(result); return result;          │
│                                                                                  │
│  3. FAULT INJECTION: fallar la N-esima llamada                                  │
│     if (call_count++ == target) return ERROR; else return __real_func();        │
│                                                                                  │
│  4. VALIDATION: verificar argumentos y delegar                                  │
│     assert_valid(args); return __real_func(args);                               │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Leak detection con CMocka

CMocka tiene soporte integrado para detectar memory leaks. Usa `test_malloc` y `test_free` (o wrappea `malloc`/`free` via `--wrap`):

### 7.1 Usando test_malloc/test_free

```c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void test_no_leak(void **state) {
    (void)state;
    // test_malloc es una version de malloc que CMocka trackea
    char *buf = test_malloc(100);
    // ... usar buf ...
    test_free(buf);
    // Al final del test, CMocka verifica que todo lo allocado fue liberado
}

static void test_with_leak(void **state) {
    (void)state;
    char *buf = test_malloc(100);
    // Olvidamos test_free(buf)
    // CMocka detectara el leak y fallara el test:
    // "Blocks allocated..."
}
```

### 7.2 Wrapping malloc/free globalmente

```c
// En el test file:
#ifdef UNIT_TESTING
extern void *_test_malloc(const size_t size, const char *file, const int line);
extern void _test_free(void *ptr, const char *file, const int line);

#define malloc(size) _test_malloc(size, __FILE__, __LINE__)
#define free(ptr) _test_free(ptr, __FILE__, __LINE__)
#endif
```

O via `--wrap`:

```bash
gcc -o test_leaks test_leaks.c mylib.c -lcmocka \
    -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc
```

```c
void *__wrap_malloc(size_t size) {
    return _test_malloc(size, __FILE__, __LINE__);
}

void __wrap_free(void *ptr) {
    _test_free(ptr, __FILE__, __LINE__);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    return _test_calloc(nmemb, size, __FILE__, __LINE__);
}
```

### 7.3 Output de leak detection

```
[==========] Running 1 test(s).
[ RUN      ] test_with_leak
[  ERROR   ] --- Blocks allocated...
              tests/test_leaks.c:15: note: block 0x55a1234 allocated here
              test_with_leak
              Size: 100
[  FAILED  ] test_with_leak
[==========] 1 test(s) run.
[  FAILED  ] 1 test(s).
```

---

## 8. Multiples wraps organizados — macros helper

Cuando tienes muchos wraps, el boilerplate crece. Macros helper lo reducen:

### 8.1 Macro para wrapper simple (retorna int)

```c
// Macro para crear un wrapper de funcion void → int
#define DEFINE_WRAP_VOID_RETURN_INT(func_name) \
    int __wrap_##func_name(void) { \
        function_called(); \
        return mock_type(int); \
    }

// Macro para wrapper con un parametro string
#define DEFINE_WRAP_STRING_RETURN_INT(func_name, param_name) \
    int __wrap_##func_name(const char *param_name) { \
        check_expected(param_name); \
        return mock_type(int); \
    }

// Uso:
DEFINE_WRAP_VOID_RETURN_INT(db_connect)
DEFINE_WRAP_STRING_RETURN_INT(db_find_table, table_name)
```

### 8.2 CMake helper para --wrap

```cmake
# Funcion CMake para agregar wraps automaticamente
function(add_cmocka_test_with_wraps TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE ${PROJECT_NAME}_lib cmocka-static)

    # Recoger los argumentos extra como funciones a wrappear
    set(WRAP_FLAGS "")
    foreach(FUNC ${ARGN})
        list(APPEND WRAP_FLAGS "-Wl,--wrap=${FUNC}")
    endforeach()

    if(WRAP_FLAGS)
        target_link_options(${TEST_NAME} PRIVATE ${WRAP_FLAGS})
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "unit;mock")
endfunction()

# Uso limpio:
add_cmocka_test_with_wraps(test_user_service
    tests/test_user_service.c
    db_find_user_by_email
    db_insert_user
    db_update_user
    db_delete_user
    send_email
    hash_password
)

add_cmocka_test_with_wraps(test_auth
    tests/test_auth.c
    db_find_user_by_email
    hash_password
    time
)
```

---

## 9. Limitacion critica: llamadas intra-object

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│  LIMITACION: LLAMADAS DENTRO DEL MISMO .o                                       │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  --wrap SOLO intercepta llamadas que CRUZAN translation units.                  │
│  Si A() y B() estan en el MISMO .c, la llamada de A a B NO se wrappea.         │
│                                                                                  │
│  // modulo.c                                                                    │
│  int helper(int x) { return x * 2; }     // helper y process en el mismo .c    │
│  int process(int x) { return helper(x); } // llamada INTRA-object              │
│                                                                                  │
│  gcc -Wl,--wrap=helper test.c modulo.c -o test                                  │
│  → process() llama al helper ORIGINAL, no al __wrap_helper                     │
│  → Porque el compilador ya resolvio la llamada DENTRO de modulo.o              │
│  → El linker no puede interceptarla (ya es un offset relativo, no un simbolo)  │
│                                                                                  │
│  Soluciones:                                                                    │
│  1. Separar helper() y process() en archivos .c diferentes                      │
│     → La llamada cruza translation units → --wrap funciona                      │
│                                                                                  │
│  2. Usar function pointers en vez de --wrap para este caso                      │
│                                                                                  │
│  3. Compilar con -fPIC (Position Independent Code) y usar                       │
│     -fno-inline para forzar llamadas via PLT                                    │
│     → Experimental, no garantizado                                              │
│                                                                                  │
│  4. Poner helper() en el header como declaracion (no definicion)                │
│     e implementarla en un .c separado                                           │
│                                                                                  │
│  En la practica: diseñar modulos con una funcion por archivo o                  │
│  al menos con las funciones "mockeables" en archivos separados.                 │
│                                                                                  │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Expectativas estrictas vs relajadas

### 10.1 Mock estricto (default de CMocka)

Por defecto, CMocka es estricto: si una funcion wrapeada se llama sin que haya expectativas configuradas, el test FALLA.

```c
static void test_strict(void **state) {
    (void)state;

    // Solo configuramos expect para db_find:
    expect_string(__wrap_db_find_user_by_email, email, "alice@test.com");
    will_return(__wrap_db_find_user_by_email, -1);

    // Si user_service_register() internamente llama a hash_password(),
    // y NO configuramos expect para hash_password,
    // → CMocka falla: "no more mock values for __wrap_hash_password"

    user_service_register("Alice", "alice@test.com", "password123");
    // FALLA porque falta configurar hash_password
}
```

### 10.2 Hacer expectativas relajadas

```c
// Opcion 1: usar expect_any_always y will_return_always para "don't care"
static void test_relaxed(void **state) {
    (void)state;

    // Solo nos importa la DB
    expect_string(__wrap_db_find_user_by_email, email, "alice@test.com");
    will_return(__wrap_db_find_user_by_email, -1);

    // El resto: aceptar cualquier cosa
    expect_any_always(__wrap_hash_password, password);
    will_return_always(__wrap_hash_password, "fakehash");
    will_return_always(__wrap_hash_password, 0);

    expect_any_always(__wrap_db_insert_user, user);
    expect_any_always(__wrap_db_insert_user, user->email);
    will_return_always(__wrap_db_insert_user, 1);

    expect_any_always(__wrap_send_email, to);
    expect_any_always(__wrap_send_email, subject);
    will_return_always(__wrap_send_email, 0);

    int id = user_service_register("Alice", "alice@test.com", "password123");
    assert_int_equal(id, 1);
}

// Opcion 2: helpers que configuran defaults
static void setup_default_mocks(void) {
    expect_any_always(__wrap_hash_password, password);
    will_return_always(__wrap_hash_password, "default_hash");
    will_return_always(__wrap_hash_password, 0);

    expect_any_always(__wrap_send_email, to);
    expect_any_always(__wrap_send_email, subject);
    will_return_always(__wrap_send_email, 0);
}
```

### 10.3 Orden de expectativas

Las expectativas se consumen en orden FIFO. Esto importa cuando wrappeas la misma funcion multiples veces:

```c
static void test_multiple_calls_ordered(void **state) {
    (void)state;

    // Primera llamada a db_find: retorna "no encontrado"
    expect_string(__wrap_db_find_user_by_email, email, "alice@test.com");
    will_return(__wrap_db_find_user_by_email, -1);

    // Segunda llamada a db_find: retorna "encontrado"
    expect_string(__wrap_db_find_user_by_email, email, "bob@test.com");
    will_return(__wrap_db_find_user_by_email, 0);
    will_return(__wrap_db_find_user_by_email, &bob_record);

    // El codigo debe llamar db_find DOS veces, en este orden.
    // Si llama primero con "bob", la expectativa de "alice" falla.
    process_batch_users();
}
```

---

## 11. Output formats — TAP y XML

CMocka puede generar output en formatos parseable por CI:

```c
// En el test o via variable de entorno:

// Opcion 1: Variable de entorno
// CMOCKA_MESSAGE_OUTPUT=TAP ./test_user_service
// CMOCKA_MESSAGE_OUTPUT=XML ./test_user_service
// CMOCKA_MESSAGE_OUTPUT=SUBUNIT ./test_user_service

// Opcion 2: Programatico
cmocka_set_message_output(CM_OUTPUT_TAP);    // Test Anything Protocol
cmocka_set_message_output(CM_OUTPUT_XML);    // XML (JUnit-like)
cmocka_set_message_output(CM_OUTPUT_SUBUNIT);// Subunit format
cmocka_set_message_output(CM_OUTPUT_STDOUT); // Default (texto)

// Opcion 3: XML a archivo
// CMOCKA_XML_FILE=results.xml ./test_user_service
```

### 11.1 TAP output

```
$ CMOCKA_MESSAGE_OUTPUT=TAP ./test_user_service
ok 1 - test_register_success
ok 2 - test_register_duplicate_email
ok 3 - test_register_invalid_email
ok 4 - test_register_short_password
ok 5 - test_register_null_name
ok 6 - test_register_db_insert_fails
ok 7 - test_register_hash_fails
ok 8 - test_register_email_failure_does_not_block
ok 9 - test_deactivate_success
ok 10 - test_deactivate_not_found
ok 11 - test_change_email_success
ok 12 - test_change_email_already_in_use
ok 13 - test_change_email_invalid
1..13
```

### 11.2 XML output para CI

```bash
CMOCKA_MESSAGE_OUTPUT=XML CMOCKA_XML_FILE=results.xml ./test_user_service
```

```xml
<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
  <testsuite name="tests" time="0.001" tests="13" failures="0" errors="0">
    <testcase name="test_register_success" time="0.000"/>
    <testcase name="test_register_duplicate_email" time="0.000"/>
    <!-- ... -->
  </testsuite>
</testsuites>
```

### 11.3 Integracion con CI

```yaml
# GitHub Actions
- name: Run CMocka tests
  run: |
    cd build
    CMOCKA_MESSAGE_OUTPUT=XML CMOCKA_XML_FILE=cmocka_results.xml \
      ctest --output-on-failure

- name: Upload test results
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: cmocka-results
    path: build/cmocka_results.xml
```

---

## 12. Comparacion con Rust mockall y Go interfaces

### 12.1 Rust mockall

```rust
// Rust con mockall: el mock se genera automaticamente desde el trait

use mockall::automock;

#[automock]
trait Database {
    fn find_user_by_email(&self, email: &str) -> Option<User>;
    fn insert_user(&self, user: &User) -> Result<i32, DbError>;
}

#[automock]
trait EmailService {
    fn send(&self, to: &str, subject: &str, body: &str) -> Result<(), Error>;
}

#[test]
fn test_register_success() {
    let mut mock_db = MockDatabase::new();
    let mut mock_email = MockEmailService::new();

    // Expectativas (equivalente a expect_* de CMocka)
    mock_db.expect_find_user_by_email()
        .with(eq("alice@test.com"))
        .times(1)
        .returning(|_| None);  // no encontrado

    mock_db.expect_insert_user()
        .times(1)
        .returning(|_| Ok(42));

    mock_email.expect_send()
        .with(eq("alice@test.com"), always(), always())
        .times(1)
        .returning(|_, _, _| Ok(()));

    let service = UserService::new(Box::new(mock_db), Box::new(mock_email));
    let result = service.register("Alice", "alice@test.com", "password123");
    assert_eq!(result, Ok(42));
    // mockall verifica automaticamente que expectations se cumplieron
}
```

### 12.2 Go gomock

```go
// Go con gomock: genera mocks desde interfaces

//go:generate mockgen -source=database.go -destination=mock_database.go

func TestRegisterSuccess(t *testing.T) {
    ctrl := gomock.NewController(t)
    defer ctrl.Finish()

    mockDB := NewMockDatabase(ctrl)
    mockEmail := NewMockEmailService(ctrl)

    // Expectativas
    mockDB.EXPECT().
        FindUserByEmail("alice@test.com").
        Return(nil, errors.New("not found")).
        Times(1)

    mockDB.EXPECT().
        InsertUser(gomock.Any()).
        Return(42, nil).
        Times(1)

    mockEmail.EXPECT().
        Send("alice@test.com", gomock.Any(), gomock.Any()).
        Return(nil).
        Times(1)

    service := NewUserService(mockDB, mockEmail)
    id, err := service.Register("Alice", "alice@test.com", "password123")
    assert.NoError(t, err)
    assert.Equal(t, 42, id)
}
```

### 12.3 Tabla comparativa

| Aspecto | C (CMocka --wrap) | Rust (mockall) | Go (gomock) |
|---------|-------------------|----------------|-------------|
| **Mock generation** | Manual (__wrap_*) | Automatico (#[automock]) | Automatico (mockgen) |
| **Expectativas** | expect_value/string | .expect_method().with() | .EXPECT().Method() |
| **Retorno** | will_return | .returning(\|\| val) | .Return(val) |
| **Times** | will_return_count | .times(n) | .Times(n) |
| **Any matcher** | expect_any | always() | gomock.Any() |
| **Verificacion** | Automatica (check_expected) | Automatica (drop) | Automatica (Finish) |
| **Leak detection** | test_malloc/test_free | Ownership (compile time) | GC (no aplica) |
| **Requiere cambio API** | No (--wrap) | Si (trait bounds) | Si (interface) |
| **Requiere cambio linker** | Si (--wrap) | No | No |
| **Portabilidad** | Solo GNU ld | Todas | Todas |
| **Invasividad** | Cero en el source | Trait bounds en generics | Interface params |

---

## 13. Errores comunes

### 13.1 "Could not get value to mock"

```
[  ERROR   ] Could not get value to mock for function __wrap_db_insert_user
```

**Causa**: el wrapper llama a `mock_type()` pero no hay `will_return()` configurado.

**Solucion**: agregar `will_return(__wrap_db_insert_user, value)` en el test.

### 13.2 "Expected parameter ... but got ..."

```
[  ERROR   ] Expected parameter 'email' of function __wrap_db_find_user_by_email
             to be string "alice@test.com" but got "bob@test.com"
```

**Causa**: el codigo llama con un argumento diferente al esperado.

**Solucion**: verificar la logica del codigo o ajustar la expectativa.

### 13.3 "Function called without expectations"

```
[  ERROR   ] Error: Could not get parameter 'email' for function
             __wrap_db_find_user_by_email
```

**Causa**: la funcion wrapeada se llamo pero no hay `expect_*` configurado.

**Solucion**: agregar expectativas o usar `expect_any_always`.

### 13.4 --wrap no funciona (la funcion real se ejecuta)

```
# Sintoma: el wrapper nunca se ejecuta
# Causa 1: olvidaste -Wl,--wrap=func_name en el linkeo
# Causa 2: la llamada es intra-object (seccion 9)
# Causa 3: el nombre del wrapper tiene un typo
#   __wrap_send_emal vs __wrap_send_email

# Diagnostico: usar nm para verificar
$ nm test_user_service | grep wrap
0000000000001234 T __wrap_send_email      # debe estar como T (defined)
                 U __real_send_email      # U = undefined (normal si no la usas)
```

### 13.5 Leak de expectativas entre tests

```c
// PROBLEMA: si un test configura expectativas pero el codigo no las consume,
// las expectativas sobrantes "leakean" al siguiente test.

static void test_A(void **state) {
    (void)state;
    will_return(__wrap_db_insert, 1);
    will_return(__wrap_db_insert, 2);
    will_return(__wrap_db_insert, 3);

    // Pero el codigo solo llama db_insert UNA vez
    // → will_return 2 y 3 quedan en la cola
    // → CMocka deberia detectar esto y fallar test_A
    // → Pero si no, test_B puede consumir los valores sobrantes
}

// SOLUCION: CMocka detecta expectativas no consumidas al final de cada test
// y falla el test. Asegurate de que cada will_return tiene su mock_type().
```

### 13.6 macOS: --wrap no esta disponible

```bash
# macOS usa el linker de Apple (ld64), no GNU ld
# --wrap no existe

# Alternativas en macOS:
# 1. Link-time substitution (T03) — funciona en todas las plataformas
# 2. Function pointer injection (T02) — funciona en todas las plataformas
# 3. DYLD_INTERPOSE — mecanismo de macOS (complejo y fragil)
# 4. Compilar en Linux (Docker) para CI con --wrap
```

---

## 14. Ejemplo completo — cliente HTTP con mocking

### 14.1 Codigo de produccion

```c
// include/http_client.h
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

typedef struct {
    int status_code;
    char body[4096];
    int body_length;
} HTTPResponse;

// Dependencias externas (a wrappear)
int tcp_connect(const char *host, int port);
int tcp_send(int socket, const char *data, int len);
int tcp_recv(int socket, char *buf, int buf_size);
void tcp_close(int socket);

// Funcion bajo test
int http_get(const char *url, HTTPResponse *response);
int http_post(const char *url, const char *body, HTTPResponse *response);

#endif
```

```c
// src/http_client.c
#include "http_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int parse_url(const char *url, char *host, int host_size,
                     int *port, char *path, int path_size) {
    // Simplificado: solo http://host:port/path
    if (strncmp(url, "http://", 7) != 0) return -1;
    const char *start = url + 7;
    const char *colon = strchr(start, ':');
    const char *slash = strchr(start, '/');

    if (colon && (!slash || colon < slash)) {
        int hlen = colon - start;
        if (hlen >= host_size) return -1;
        memcpy(host, start, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else {
        *port = 80;
        int hlen = slash ? (slash - start) : (int)strlen(start);
        if (hlen >= host_size) return -1;
        memcpy(host, start, hlen);
        host[hlen] = '\0';
    }

    if (slash) {
        snprintf(path, path_size, "%s", slash);
    } else {
        snprintf(path, path_size, "/");
    }

    return 0;
}

int http_get(const char *url, HTTPResponse *response) {
    if (!url || !response) return -1;
    memset(response, 0, sizeof(*response));

    char host[256], path[256];
    int port;
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return -1;
    }

    int sock = tcp_connect(host, port);
    if (sock < 0) return -1;

    char request[512];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);

    if (tcp_send(sock, request, req_len) != req_len) {
        tcp_close(sock);
        return -1;
    }

    char raw_response[8192];
    int received = tcp_recv(sock, raw_response, sizeof(raw_response) - 1);
    tcp_close(sock);

    if (received <= 0) return -1;
    raw_response[received] = '\0';

    // Parse status line: "HTTP/1.1 200 OK\r\n..."
    if (sscanf(raw_response, "HTTP/%*d.%*d %d", &response->status_code) != 1) {
        return -1;
    }

    // Find body (after \r\n\r\n)
    char *body_start = strstr(raw_response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int body_len = received - (body_start - raw_response);
        if (body_len >= (int)sizeof(response->body)) {
            body_len = sizeof(response->body) - 1;
        }
        memcpy(response->body, body_start, body_len);
        response->body[body_len] = '\0';
        response->body_length = body_len;
    }

    return 0;
}

int http_post(const char *url, const char *body, HTTPResponse *response) {
    if (!url || !body || !response) return -1;
    memset(response, 0, sizeof(*response));

    char host[256], path[256];
    int port;
    if (parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0) {
        return -1;
    }

    int sock = tcp_connect(host, port);
    if (sock < 0) return -1;

    int body_len = strlen(body);
    char request[4096];
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        path, host, body_len, body);

    if (tcp_send(sock, request, req_len) != req_len) {
        tcp_close(sock);
        return -1;
    }

    char raw_response[8192];
    int received = tcp_recv(sock, raw_response, sizeof(raw_response) - 1);
    tcp_close(sock);

    if (received <= 0) return -1;
    raw_response[received] = '\0';

    if (sscanf(raw_response, "HTTP/%*d.%*d %d", &response->status_code) != 1) {
        return -1;
    }

    char *body_start = strstr(raw_response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        int rlen = received - (body_start - raw_response);
        if (rlen >= (int)sizeof(response->body)) rlen = sizeof(response->body) - 1;
        memcpy(response->body, body_start, rlen);
        response->body[rlen] = '\0';
        response->body_length = rlen;
    }

    return 0;
}
```

### 14.2 Tests con wrappers CMocka

```c
// tests/test_http_client.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "http_client.h"

// ==================== WRAPPERS ====================

int __wrap_tcp_connect(const char *host, int port) {
    check_expected(host);
    check_expected(port);
    return mock_type(int);  // socket fd or -1
}

int __wrap_tcp_send(int socket, const char *data, int len) {
    check_expected(socket);
    // No verificamos data exacto (muy fragil), solo que no es NULL
    check_expected_ptr(data);
    check_expected(len);
    return mock_type(int);
}

int __wrap_tcp_recv(int socket, char *buf, int buf_size) {
    check_expected(socket);

    const char *mock_response = mock_ptr_type(const char *);
    int mock_len = mock_type(int);

    if (mock_response && mock_len > 0) {
        int to_copy = mock_len < buf_size ? mock_len : buf_size;
        memcpy(buf, mock_response, to_copy);
        return to_copy;
    }
    return mock_len;  // -1 for error, 0 for no data
}

void __wrap_tcp_close(int socket) {
    check_expected(socket);
    // void function: no mock_type needed
}

// ==================== HELPERS ====================

static void expect_connect(const char *host, int port, int socket_fd) {
    expect_string(__wrap_tcp_connect, host, host);
    expect_value(__wrap_tcp_connect, port, port);
    will_return(__wrap_tcp_connect, socket_fd);
}

static void expect_send_ok(int socket_fd) {
    expect_value(__wrap_tcp_send, socket, socket_fd);
    expect_any(__wrap_tcp_send, data);
    expect_any(__wrap_tcp_send, len);
    // Retornar el mismo len que se envio (simulacion de envio completo)
    will_return(__wrap_tcp_send, 512);  // aproximado
}

static void expect_recv(int socket_fd, const char *response) {
    expect_value(__wrap_tcp_recv, socket, socket_fd);
    will_return(__wrap_tcp_recv, response);
    will_return(__wrap_tcp_recv, (int)strlen(response));
}

static void expect_close(int socket_fd) {
    expect_value(__wrap_tcp_close, socket, socket_fd);
}

// ==================== TESTS ====================

static void test_http_get_success(void **state) {
    (void)state;

    expect_connect("example.com", 80, 5);
    expect_send_ok(5);
    expect_recv(5, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello World");
    expect_close(5);

    HTTPResponse resp;
    int rc = http_get("http://example.com/hello", &resp);

    assert_int_equal(rc, 0);
    assert_int_equal(resp.status_code, 200);
    assert_string_equal(resp.body, "Hello World");
}

static void test_http_get_404(void **state) {
    (void)state;

    expect_connect("api.test.com", 8080, 7);
    expect_send_ok(7);
    expect_recv(7, "HTTP/1.1 404 Not Found\r\n\r\n{\"error\":\"not found\"}");
    expect_close(7);

    HTTPResponse resp;
    int rc = http_get("http://api.test.com:8080/users/999", &resp);

    assert_int_equal(rc, 0);
    assert_int_equal(resp.status_code, 404);
    assert_string_equal(resp.body, "{\"error\":\"not found\"}");
}

static void test_http_get_connect_fails(void **state) {
    (void)state;

    expect_connect("down.server.com", 80, -1);  // conexion fallida

    HTTPResponse resp;
    int rc = http_get("http://down.server.com/", &resp);

    assert_int_equal(rc, -1);
}

static void test_http_get_recv_fails(void **state) {
    (void)state;

    expect_connect("example.com", 80, 3);
    expect_send_ok(3);
    // recv retorna error
    expect_value(__wrap_tcp_recv, socket, 3);
    will_return(__wrap_tcp_recv, NULL);
    will_return(__wrap_tcp_recv, -1);
    expect_close(3);

    HTTPResponse resp;
    int rc = http_get("http://example.com/", &resp);

    assert_int_equal(rc, -1);
}

static void test_http_get_invalid_url(void **state) {
    (void)state;

    HTTPResponse resp;
    assert_int_equal(http_get("ftp://wrong.protocol/", &resp), -1);
    assert_int_equal(http_get(NULL, &resp), -1);
}

static void test_http_post_success(void **state) {
    (void)state;

    expect_connect("api.test.com", 80, 10);
    expect_send_ok(10);
    expect_recv(10, "HTTP/1.1 201 Created\r\n\r\n{\"id\":42}");
    expect_close(10);

    HTTPResponse resp;
    int rc = http_post("http://api.test.com/users",
                       "{\"name\":\"Alice\"}", &resp);

    assert_int_equal(rc, 0);
    assert_int_equal(resp.status_code, 201);
    assert_string_equal(resp.body, "{\"id\":42}");
}

static void test_http_post_server_error(void **state) {
    (void)state;

    expect_connect("api.test.com", 80, 11);
    expect_send_ok(11);
    expect_recv(11, "HTTP/1.1 500 Internal Server Error\r\n\r\n{\"error\":\"boom\"}");
    expect_close(11);

    HTTPResponse resp;
    int rc = http_post("http://api.test.com/users",
                       "{\"name\":\"Alice\"}", &resp);

    assert_int_equal(rc, 0);
    assert_int_equal(resp.status_code, 500);
}

// ==================== MAIN ====================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_http_get_success),
        cmocka_unit_test(test_http_get_404),
        cmocka_unit_test(test_http_get_connect_fails),
        cmocka_unit_test(test_http_get_recv_fails),
        cmocka_unit_test(test_http_get_invalid_url),
        cmocka_unit_test(test_http_post_success),
        cmocka_unit_test(test_http_post_server_error),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 14.3 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(http_client_test C)
set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(cmocka
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG cmocka-1.1.7
)
set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cmocka)

add_library(http_client src/http_client.c)
target_include_directories(http_client PUBLIC include)

enable_testing()

add_executable(test_http_client tests/test_http_client.c)
target_link_libraries(test_http_client PRIVATE http_client cmocka-static)
target_link_options(test_http_client PRIVATE
    -Wl,--wrap=tcp_connect
    -Wl,--wrap=tcp_send
    -Wl,--wrap=tcp_recv
    -Wl,--wrap=tcp_close
)

add_test(NAME test_http_client COMMAND test_http_client)
set_tests_properties(test_http_client PROPERTIES LABELS "unit;mock;network")
```

---

## 15. Programa de practica — cliente de API REST con mocking completo

### 15.1 Requisitos

Implementa un cliente de API REST que interactua con un servicio externo:

```c
// api_client.h

// Dependencias externas (a wrappear con CMocka)
int tcp_connect(const char *host, int port);
int tcp_send(int socket, const char *data, int len);
int tcp_recv(int socket, char *buf, int buf_size);
void tcp_close(int socket);
time_t get_current_time(void);

// Modelos
typedef struct {
    int id;
    char title[128];
    char body[1024];
    int user_id;
} Post;

// API client
typedef struct {
    char base_url[256];
    char api_key[64];
    int timeout_seconds;
} APIClient;

APIClient api_client_new(const char *base_url, const char *api_key);

// Operaciones
int api_get_post(APIClient *c, int post_id, Post *out);
int api_create_post(APIClient *c, const Post *post);
int api_list_posts(APIClient *c, int user_id, Post *out, int max_posts);
int api_delete_post(APIClient *c, int post_id);
```

### 15.2 Lo que debes cubrir

Tests con CMocka wrapping (15+ tests):
1. GET post exitoso — response 200 con JSON parseado
2. GET post not found — response 404
3. GET post — connection failure
4. GET post — timeout (recv retorna 0)
5. GET post — invalid JSON en response
6. CREATE post exitoso — response 201
7. CREATE post — server error 500
8. CREATE post — verifica que el api_key se envio en el header
9. LIST posts — multiples posts en response
10. LIST posts — lista vacia
11. DELETE post — response 204
12. DELETE post — response 403 forbidden
13. Request incluye el header correcto (Host, Authorization)
14. URL parsing correcto con diferentes formatos
15. Manejo de conexion cerrada prematuramente

Usa **helpers** (como los de la seccion 4.2) para reducir boilerplate. Usa **expect_string** para verificar que el host y api_key son correctos.

---

## 16. Ejercicios

### Ejercicio 1: Fault injection con __real_

Wrappea `malloc` y crea un mecanismo de fault injection que falle la N-esima asignacion. Usa `__real_malloc` para las asignaciones que deben funcionar. Testea que tu libreria maneja correctamente:
- La 1ra asignacion falla
- La 5ta asignacion falla (las primeras 4 funcionan)
- Ninguna asignacion falla (todas usan `__real_malloc`)

### Ejercicio 2: Mock de time() para rate limiter

Implementa un rate limiter: `bool rate_limit_check(const char *ip, int max_per_minute)`. Usa `--wrap=time` para controlar el reloj. Testea:
- Primera llamada siempre pasa
- N+1 llamada en el mismo minuto falla
- Llamada despues de 60 segundos pasa (ventana se resetea)
- Diferentes IPs tienen contadores independientes

### Ejercicio 3: Spy con __real_ passthrough

Crea un wrapper de `write()` (POSIX) que grabe todas las escrituras a un archivo especifico (`/var/log/app.log`) pero deje pasar todas las demas escrituras a la implementacion real via `__real_write()`. Testea que tu aplicacion escribe los mensajes correctos al log sin interferir con otras escrituras (stdout, stderr).

### Ejercicio 4: Comparar las tres tecnicas

Para la misma dependencia (`database.h`), implementa el test usando:
1. Function pointer injection (T02)
2. Link-time substitution (T03)
3. CMocka --wrap (este topico)

Documenta para cada uno:
- Lineas de codigo del test
- Lineas de boilerplate (setup que no es logica de test)
- ¿Necesitas cambiar el codigo de produccion?
- ¿Necesitas cambiar el build system?
- ¿Puedes verificar argumentos de las llamadas?
- ¿Puedes cambiar el mock entre tests sin recompilar?

---

Navegacion:

- Anterior: [T03 — Link-time substitution](../T03_Link_time_substitution/README.md)
- Siguiente: [S04/T01 — gcov y lcov](../../S04_Cobertura_en_C/T01_gcov_y_lcov/README.md)
- Indice: [BLOQUE 17 — Testing e Ingenieria de Software](../../../BLOQUE_17.md)

---

*Fin de la Seccion 3 — Mocking y Test Doubles en C*

*Proximo topico: S04/T01 — gcov y lcov*
