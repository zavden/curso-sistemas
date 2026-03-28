# Variables de Entorno — La Configuración Heredada por Procesos

## Índice

1. [Qué son las variables de entorno](#1-qué-son-las-variables-de-entorno)
2. [Representación interna: environ](#2-representación-interna-environ)
3. [getenv: leer variables](#3-getenv-leer-variables)
4. [setenv y putenv: modificar variables](#4-setenv-y-putenv-modificar-variables)
5. [unsetenv y clearenv: eliminar variables](#5-unsetenv-y-clearenv-eliminar-variables)
6. [Herencia en fork y exec](#6-herencia-en-fork-y-exec)
7. [Entorno custom con execle/execve](#7-entorno-custom-con-execleexecve)
8. [Variables estándar del sistema](#8-variables-estándar-del-sistema)
9. [Seguridad](#9-seguridad)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué son las variables de entorno

Las variables de entorno son pares **clave=valor** que el kernel pasa a
cada proceso al inicio. Son el mecanismo estándar de Unix para configurar
programas sin modificar su código ni sus argumentos:

```
  ┌──────────────────────────────────────────┐
  │           Proceso (PID 1234)             │
  │                                          │
  │  argv[]    → argumentos de línea         │
  │  environ[] → variables de entorno        │
  │                                          │
  │  PATH=/usr/bin:/bin                      │
  │  HOME=/home/user                         │
  │  LANG=es_ES.UTF-8                        │
  │  EDITOR=vim                              │
  │  DATABASE_URL=postgres://localhost/mydb  │
  └──────────────────────────────────────────┘
```

### Dónde se originan

```
  1. Kernel pasa entorno inicial mínimo
          │
  2. init/systemd añade variables de servicio
          │
  3. Shell de login ejecuta /etc/profile, ~/.bash_profile
          │
  4. Shell interactivo ejecuta ~/.bashrc
          │
  5. El proceso padre pasa su entorno (o uno custom) al hijo
          │
     ┌────┴────┐
     │ fork()  │  → hijo hereda copia completa
     │ exec()  │  → nuevo programa recibe el entorno
     └─────────┘
```

### Características clave

- Son **strings**: siempre `char *`, nunca tipos numéricos ni estructurados.
- Son **por proceso**: cada proceso tiene su propia copia.
- Se heredan en `fork` y `exec` por defecto.
- El nombre es **case-sensitive** en Unix (`PATH` ≠ `path`).
- Por convención, los nombres son MAYÚSCULAS.

---

## 2. Representación interna: environ

El entorno de un proceso es un array de strings terminado en `NULL`,
accesible a través de la variable global `environ`:

```c
#include <stdio.h>

extern char **environ;  /* Declared in <unistd.h> on most systems */

int main(void)
{
    for (char **ep = environ; *ep != NULL; ep++) {
        printf("%s\n", *ep);
    }
    return 0;
}
```

```
  $ ./print_env | head -5
  SHELL=/bin/zsh
  HOME=/home/user
  PATH=/usr/local/bin:/usr/bin:/bin
  LANG=es_ES.UTF-8
  TERM=xterm-256color
```

### Estructura en memoria

```
  environ (char **)
  ┌─────────────┐
  │ environ[0]  │──▶ "SHELL=/bin/zsh\0"
  │ environ[1]  │──▶ "HOME=/home/user\0"
  │ environ[2]  │──▶ "PATH=/usr/local/bin:/usr/bin:/bin\0"
  │ environ[3]  │──▶ "LANG=es_ES.UTF-8\0"
  │   ...       │
  │ environ[N]  │──▶ NULL  (terminador)
  └─────────────┘
```

Cada string tiene el formato `"NAME=value"`. El primer `=` separa nombre
de valor. Un nombre puede contener letras, dígitos y `_` (no debe empezar
con dígito). El valor puede contener cualquier carácter excepto `\0`.

### Tercer argumento de main (no estándar)

Algunos sistemas permiten recibir el entorno como tercer argumento de
`main`:

```c
/* Non-standard but widely supported */
int main(int argc, char *argv[], char *envp[])
{
    for (int i = 0; envp[i] != NULL; i++)
        printf("%s\n", envp[i]);
    return 0;
}
```

Esto **no** es POSIX estándar. Usar `extern char **environ` es la forma
portable.

---

## 3. getenv: leer variables

```c
#include <stdlib.h>

char *getenv(const char *name);
```

- Devuelve un puntero al **valor** (la parte después del `=`).
- Devuelve `NULL` si la variable no existe.
- El puntero apunta **dentro** del array environ — no es una copia.

### Uso básico

```c
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "HOME not set\n");
        return 1;
    }
    printf("home directory: %s\n", home);

    const char *editor = getenv("EDITOR");
    printf("editor: %s\n", editor ? editor : "not set");

    return 0;
}
```

### Patrón: valor por defecto

```c
const char *get_config(const char *name, const char *default_val)
{
    const char *val = getenv(name);
    return val ? val : default_val;
}

/* Usage */
const char *port = get_config("PORT", "8080");
const char *host = get_config("HOST", "0.0.0.0");
const char *log_level = get_config("LOG_LEVEL", "info");
```

### Patrón: convertir a entero

```c
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int get_env_int(const char *name, int default_val)
{
    const char *val = getenv(name);
    if (val == NULL)
        return default_val;

    char *end;
    errno = 0;
    long n = strtol(val, &end, 10);

    if (errno != 0 || *end != '\0' || n < INT_MIN || n > INT_MAX) {
        fprintf(stderr, "warning: invalid %s=%s, using default %d\n",
                name, val, default_val);
        return default_val;
    }

    return (int)n;
}

/* Usage */
int port = get_env_int("PORT", 8080);
int workers = get_env_int("WORKERS", 4);
int timeout = get_env_int("TIMEOUT_SECS", 30);
```

### Patrón: boolean

```c
int get_env_bool(const char *name, int default_val)
{
    const char *val = getenv(name);
    if (val == NULL)
        return default_val;

    /* "1", "true", "yes" → true. Anything else → false */
    return (val[0] == '1' || val[0] == 't' || val[0] == 'T' ||
            val[0] == 'y' || val[0] == 'Y');
}

int debug = get_env_bool("DEBUG", 0);
int verbose = get_env_bool("VERBOSE", 0);
```

---

## 4. setenv y putenv: modificar variables

### setenv

```c
#include <stdlib.h>

int setenv(const char *name, const char *value, int overwrite);
```

- `name`: nombre de la variable (sin `=`).
- `value`: nuevo valor.
- `overwrite`: si la variable ya existe, solo se modifica si `overwrite != 0`.
- Retorna `0` en éxito, `-1` en error.
- `setenv` **copia** name y value a memoria propia.

```c
/* Set a new variable */
setenv("MY_APP_PORT", "9090", 1);

/* Set only if not already defined */
setenv("LANG", "en_US.UTF-8", 0);  /* No-op if LANG already exists */

/* Always overwrite */
setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1);
```

### putenv (legacy)

```c
#include <stdlib.h>

int putenv(char *string);  /* Note: NOT const */
```

- Recibe una string con formato `"NAME=value"`.
- **No copia** la string — inserta el puntero directamente en environ.
- La string debe sobrevivir mientras la variable exista.

```c
/* PELIGRO: la string debe persistir */
static char myvar[] = "MY_VAR=hello";  /* static! */
putenv(myvar);

/* Si cambias myvar, cambias la variable de entorno: */
myvar[7] = 'H';  /* MY_VAR ahora es "Hello" */
```

> **Recomendación**: Usar `setenv` siempre. `putenv` existe por razones
> históricas y es propenso a errores de memoria.

### Modificar environ directamente

Es posible pero no recomendado:

```c
/* Technically valid but fragile */
extern char **environ;
environ = my_custom_env;  /* Replace entire environment */
```

---

## 5. unsetenv y clearenv: eliminar variables

### unsetenv

```c
#include <stdlib.h>

int unsetenv(const char *name);
```

Elimina la variable del entorno. Si no existe, no es un error (retorna 0):

```c
unsetenv("SECRET_TOKEN");  /* Remove sensitive data */
unsetenv("LD_PRELOAD");    /* Security: remove library injection */
```

### clearenv (no POSIX)

```c
#define _GNU_SOURCE
#include <stdlib.h>

int clearenv(void);
```

Elimina **todas** las variables de entorno. Útil para construir un entorno
limpio antes de exec:

```c
/* Start with clean environment */
clearenv();
setenv("PATH", "/usr/bin:/bin", 1);
setenv("HOME", "/home/user", 1);
setenv("TERM", "xterm-256color", 1);
```

Si `clearenv` no está disponible (no es POSIX):

```c
extern char **environ;
environ = NULL;  /* Equivalent on most implementations */

/* Or the portable way: */
while (environ && *environ)
    unsetenv(/* parse name from *environ */);
```

---

## 6. Herencia en fork y exec

### fork: copia completa

Después de `fork`, el hijo tiene una **copia independiente** del entorno.
Modificar el entorno en el padre no afecta al hijo, y viceversa:

```c
setenv("SHARED", "original", 1);

pid_t pid = fork();

if (pid == 0) {
    /* Child: modify its own copy */
    setenv("SHARED", "modified_by_child", 1);
    printf("child sees: %s\n", getenv("SHARED"));
    /* Output: modified_by_child */
    _exit(0);
}

waitpid(pid, NULL, 0);
printf("parent sees: %s\n", getenv("SHARED"));
/* Output: original — parent's copy is unaffected */
```

### exec: herencia por defecto

Por defecto, `execvp`/`execv`/`execlp`/`execl` heredan el entorno del
proceso actual (el array `environ`):

```c
if (pid == 0) {
    /* Modify environment before exec — child program will see it */
    setenv("DATABASE_URL", "postgres://localhost/mydb", 1);
    unsetenv("SECRET_KEY");  /* Don't leak to child */

    execvp("my_server", argv);
    _exit(127);
}
```

```
  Padre                         Hijo (después de exec)
  ──────                        ──────────────────────
  PATH=/usr/bin:/bin            PATH=/usr/bin:/bin        ← heredada
  HOME=/home/user               HOME=/home/user           ← heredada
  SECRET=abc123                 (no existe)               ← eliminada
  DATABASE_URL=(no existía)     DATABASE_URL=postgres://  ← añadida
```

---

## 7. Entorno custom con execle/execve

Las variantes `execle` y `execve` permiten pasar un entorno **custom**
en lugar de heredar `environ`:

```c
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        /* Build a minimal, controlled environment */
        char *envp[] = {
            "PATH=/usr/bin:/bin",
            "HOME=/home/app",
            "LANG=C",
            "APP_CONFIG=/etc/myapp/config.ini",
            NULL
        };

        char *argv[] = {"my_server", "--port", "8080", NULL};

        execve("/usr/local/bin/my_server", argv, envp);
        _exit(127);
    }

    /* ... wait ... */
}
```

### Cuándo usar entorno custom

| Situación                                  | Enfoque                     |
|-------------------------------------------|-----------------------------|
| Necesitas añadir/modificar pocas variables | `setenv` + `execvp`         |
| Necesitas un entorno minimal y controlado  | `execve` con `envp` custom  |
| Ejecutas código no confiable               | `execve` con `envp` mínimo  |
| Drop de privilegios (setuid programs)      | `execve` con `envp` limpio  |

### Patrón: entorno base + variables extra

```c
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Build environment = current environ + extras */
char **build_env(const char *extras[], int nextra)
{
    extern char **environ;

    /* Count current environment */
    int nenv = 0;
    while (environ[nenv]) nenv++;

    char **newenv = malloc((nenv + nextra + 1) * sizeof(char *));

    /* Copy current */
    for (int i = 0; i < nenv; i++)
        newenv[i] = environ[i];

    /* Append extras */
    for (int i = 0; i < nextra; i++)
        newenv[nenv + i] = (char *)extras[i];

    newenv[nenv + nextra] = NULL;
    return newenv;
}

/* Usage */
const char *extras[] = {
    "APP_MODE=production",
    "APP_PORT=9090",
};
char **env = build_env(extras, 2);
execve("/usr/local/bin/app", argv, env);
```

> **Nota**: Este enfoque no maneja duplicados (si `APP_PORT` ya estaba en
> `environ`, ahora aparece dos veces). En ese caso, la mayoría de
> implementaciones usan la **primera** ocurrencia.

---

## 8. Variables estándar del sistema

### Variables POSIX obligatorias/comunes

| Variable   | Descripción                           | Ejemplo                    |
|-----------|---------------------------------------|----------------------------|
| `HOME`    | Directorio home del usuario           | `/home/user`               |
| `PATH`    | Directorios de búsqueda de programas  | `/usr/bin:/bin`             |
| `SHELL`   | Shell de login del usuario            | `/bin/bash`                |
| `USER`    | Nombre del usuario                    | `user`                     |
| `LOGNAME` | Nombre de login (similar a USER)      | `user`                     |
| `TERM`    | Tipo de terminal                      | `xterm-256color`           |
| `LANG`    | Locale del sistema                    | `es_ES.UTF-8`              |
| `PWD`     | Directorio actual                     | `/home/user/project`       |
| `OLDPWD`  | Directorio anterior (para `cd -`)     | `/home/user`               |
| `TZ`      | Zona horaria                          | `Europe/Madrid`            |
| `TMPDIR`  | Directorio para archivos temporales   | `/tmp`                     |

### Variables de compilación/desarrollo

| Variable          | Descripción                            |
|-------------------|----------------------------------------|
| `CC`              | Compilador de C                        |
| `CFLAGS`          | Flags de compilación                   |
| `LDFLAGS`         | Flags del linker                       |
| `PKG_CONFIG_PATH` | Búsqueda de pkg-config                 |
| `LD_LIBRARY_PATH` | Búsqueda de bibliotecas compartidas    |
| `LD_PRELOAD`      | Bibliotecas a cargar antes que todas   |

### Variables de aplicación (convenciones)

```c
/* 12-factor app style: all config via environment */
const char *db_url   = getenv("DATABASE_URL");
const char *redis    = getenv("REDIS_URL");
const char *port     = getenv("PORT");
const char *api_key  = getenv("API_KEY");
const char *log_fmt  = getenv("LOG_FORMAT");  /* "json" or "text" */
```

La metodología **12-factor** recomienda que toda configuración que varía
entre deploys (desarrollo, staging, producción) se pase por variables
de entorno.

---

## 9. Seguridad

### El entorno es visible

Cualquier proceso puede leer su propio entorno, y **root puede leer el
de cualquier proceso**:

```bash
# Read environment of PID 1234
cat /proc/1234/environ | tr '\0' '\n'

# Or with ps
ps eww -p 1234
```

Esto significa que las variables de entorno **no son secretas** frente a
root o frente a cualquiera con acceso a `/proc`. Son mejores que
argumentos de línea de comandos (que son visibles con `ps aux`), pero
no son un almacén seguro.

### Variables peligrosas

Algunas variables modifican el comportamiento del runtime de forma que
un atacante puede explotar:

| Variable          | Riesgo                                    |
|-------------------|-------------------------------------------|
| `LD_PRELOAD`      | Inyecta bibliotecas en cualquier programa |
| `LD_LIBRARY_PATH` | Redirige la carga de .so                  |
| `PATH`            | Ejecuta programas falsos                  |
| `IFS`             | Altera el parsing del shell               |
| `TMPDIR`          | Redirige archivos temporales              |

### Programas setuid y el dynamic linker

El dynamic linker (`ld.so`) **ignora** `LD_PRELOAD` y `LD_LIBRARY_PATH`
en programas setuid/setgid. Esto es una protección de seguridad del kernel.

### Limpiar el entorno en programas privilegiados

```c
#include <stdlib.h>
#include <string.h>

/* Whitelist approach: only keep known-safe variables */
void sanitize_environment(void)
{
    static const char *safe_vars[] = {
        "PATH", "HOME", "LANG", "TERM", "TZ", "USER", NULL
    };

    /* Save safe values */
    char *saved[16] = {0};
    for (int i = 0; safe_vars[i]; i++) {
        const char *v = getenv(safe_vars[i]);
        if (v) saved[i] = strdup(v);
    }

    /* Clear everything */
    extern char **environ;
    environ = NULL;

    /* Restore safe variables */
    for (int i = 0; safe_vars[i]; i++) {
        if (saved[i]) {
            setenv(safe_vars[i], saved[i], 1);
            free(saved[i]);
        }
    }

    /* Force safe PATH */
    setenv("PATH", "/usr/bin:/bin", 1);
}
```

### secure_getenv (GNU extension)

```c
#define _GNU_SOURCE
#include <stdlib.h>

char *secure_getenv(const char *name);
```

`secure_getenv` devuelve `NULL` si el proceso es setuid/setgid, incluso
si la variable existe. Evita que programas privilegiados confíen en
variables definidas por un usuario no privilegiado:

```c
/* In a setuid program: */
const char *config = secure_getenv("MY_CONFIG");
/* Returns NULL if running with elevated privileges,
   even if MY_CONFIG is defined */
```

---

## 10. Errores comunes

### Error 1: Guardar el puntero de getenv tras modificar el entorno

```c
/* MAL: el puntero puede invalidarse */
char *path = getenv("PATH");
setenv("PATH", "/new/path", 1);  /* Puede reubicar environ */
printf("%s\n", path);            /* Undefined behavior: path may be dangling */

/* BIEN: copiar el valor */
const char *tmp = getenv("PATH");
char *path = tmp ? strdup(tmp) : NULL;
setenv("PATH", "/new/path", 1);
printf("%s\n", path);  /* Safe — it's our own copy */
free(path);
```

`getenv` devuelve un puntero dentro de `environ`. Si `setenv` necesita
expandir el array, puede mover la memoria. El puntero anterior queda
colgando.

### Error 2: Usar putenv con variables locales

```c
/* MAL: la variable local se destruye al salir de la función */
void setup(void)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "PORT=%d", 8080);
    putenv(buf);  /* environ now points into stack frame */
}
/* After setup() returns, the stack is reused → environ has garbage */

/* BIEN: usar setenv (que copia) */
void setup(void)
{
    char val[16];
    snprintf(val, sizeof(val), "%d", 8080);
    setenv("PORT", val, 1);  /* setenv copies the value */
}
```

### Error 3: No validar valores del entorno

```c
/* MAL: confiar ciegamente en el entorno */
int port = atoi(getenv("PORT"));  /* Crash if PORT is NULL (atoi(NULL)) */

/* BIEN: verificar existencia y validar */
const char *port_str = getenv("PORT");
if (port_str == NULL) {
    fprintf(stderr, "PORT not set, using default 8080\n");
    port_str = "8080";
}
char *end;
long port = strtol(port_str, &end, 10);
if (*end != '\0' || port < 1 || port > 65535) {
    fprintf(stderr, "invalid PORT: %s\n", port_str);
    return 1;
}
```

### Error 4: Filtrar secretos a hijos vía herencia

```c
/* MAL: el hijo hereda TODAS las variables, incluidos secretos */
setenv("DB_PASSWORD", "s3cret", 1);
if (fork() == 0) {
    execvp("untrusted_plugin", argv);
    /* The plugin can read DB_PASSWORD via getenv */
    _exit(127);
}

/* BIEN: eliminar secretos antes de exec */
if (fork() == 0) {
    unsetenv("DB_PASSWORD");
    unsetenv("API_SECRET");
    execvp("untrusted_plugin", argv);
    _exit(127);
}

/* MEJOR: usar execve con entorno explícito */
if (fork() == 0) {
    char *clean_env[] = {
        "PATH=/usr/bin:/bin",
        "HOME=/tmp",
        NULL
    };
    execve("/usr/local/bin/plugin", argv, clean_env);
    _exit(127);
}
```

### Error 5: Thread safety — setenv en programas multihilo

```c
/* MAL: setenv/getenv no son thread-safe entre sí */
/* Thread A: */ setenv("KEY", "value_a", 1);
/* Thread B: */ char *v = getenv("KEY");  /* Race condition */

/* El estándar POSIX dice que setenv y getenv no necesitan ser
   thread-safe simultáneamente. En glibc moderna hay un lock
   interno, pero no está garantizado en todas las implementaciones. */

/* BIEN: inicializar el entorno ANTES de crear threads,
   o proteger con mutex propio */
pthread_mutex_lock(&env_mutex);
setenv("KEY", "value", 1);
pthread_mutex_unlock(&env_mutex);
```

POSIX no requiere que `setenv`/`getenv` sean thread-safe entre sí.
La mejor práctica es configurar todas las variables de entorno **antes**
de crear threads, y que los threads solo usen `getenv` (lectura).

---

## 11. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │               Variables de Entorno                           │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Leer:                                                       │
  │    getenv("NAME")             → valor o NULL                 │
  │    secure_getenv("NAME")      → NULL si setuid (GNU)         │
  │    extern char **environ      → array "KEY=val" terminado    │
  │                                  en NULL                     │
  │                                                              │
  │  Escribir:                                                   │
  │    setenv("NAME","val",1)     → copia, overwrite=1           │
  │    setenv("NAME","val",0)     → no sobrescribe si existe     │
  │    putenv("NAME=val")         → NO copia (legacy, evitar)    │
  │                                                              │
  │  Eliminar:                                                   │
  │    unsetenv("NAME")           → elimina variable             │
  │    clearenv()                 → elimina todo (GNU)           │
  │                                                              │
  │  Herencia:                                                   │
  │    fork()   → hijo recibe copia independiente                │
  │    execvp() → hereda environ actual                          │
  │    execve(path, argv, envp)   → usa envp custom              │
  │                                                              │
  │  Seguridad:                                                  │
  │    • /proc/PID/environ es legible → no son secretos          │
  │    • LD_PRELOAD ignorado en setuid                           │
  │    • Whitelist approach antes de exec no confiable            │
  │    • Inicializar antes de crear threads                      │
  │                                                              │
  │  Shell:                                                      │
  │    env                        listar variables               │
  │    export VAR=val             definir + exportar              │
  │    VAR=val cmd                solo para ese comando           │
  │    env -i cmd                 ejecutar con entorno vacío      │
  │    printenv VAR               imprimir una variable           │
  │    unset VAR                  eliminar                        │
  │                                                              │
  │  /proc/PID/environ            entorno del proceso (NUL-sep)  │
  └──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Config loader desde entorno

Escribe un programa que cargue su configuración enteramente desde variables
de entorno, siguiendo el estilo 12-factor:

```c
typedef struct {
    char     host[256];
    int      port;
    char     db_url[512];
    int      workers;
    int      debug;
    char     log_level[16];  /* "debug", "info", "warn", "error" */
} app_config_t;

int load_config(app_config_t *cfg);
```

Requisitos:
1. `APP_HOST` (default `"0.0.0.0"`), `APP_PORT` (default `8080`).
2. `DATABASE_URL` — **obligatoria**, error si no existe.
3. `APP_WORKERS` (default `4`), validar que sea entre 1 y 64.
4. `APP_DEBUG` (default `0`), acepta `"1"`, `"true"`, `"yes"`.
5. `APP_LOG_LEVEL` (default `"info"`), validar contra lista.
6. Imprimir toda la configuración cargada (ocultar `DATABASE_URL` parcialmente:
   mostrar solo el host, no la contraseña).

**Predicción antes de codificar**: Si `APP_PORT=abc`, ¿qué debería hacer
tu programa? ¿Usar el default o reportar error? Justifica tu elección.

**Pregunta de reflexión**: ¿Por qué la metodología 12-factor prefiere
variables de entorno sobre archivos de configuración como `config.ini`?
¿En qué escenarios un archivo podría ser mejor?

---

### Ejercicio 2: env seguro para exec

Escribe una función `safe_exec` que lance un programa con un entorno
sanitizado:

```c
int safe_exec(const char *path, char *const argv[],
              const char *allowed_vars[],  /* Whitelist from parent */
              const char *extra_vars[]);   /* Additional KEY=val */
```

Comportamiento:
1. El hijo solo hereda las variables que estén en `allowed_vars[]`.
2. Añade las variables de `extra_vars[]` (formato `"KEY=val"`).
3. Si `PATH` no está en allowed ni en extra, añade `/usr/bin:/bin`.
4. Imprime en stderr qué variables se **eliminaron** (para auditoría).

Probar con:

```c
const char *allowed[] = {"HOME", "LANG", "TERM", NULL};
const char *extra[]   = {"APP_MODE=sandbox", NULL};
safe_exec("/usr/bin/env", (char *[]){"env", NULL}, allowed, extra);
```

**Predicción antes de codificar**: Si el padre tiene 50 variables de
entorno y `allowed` solo lista 3, ¿cuántas verá el hijo? ¿Cuáles?

**Pregunta de reflexión**: ¿Un enfoque whitelist (solo pasar lo permitido)
es más seguro que un enfoque blacklist (quitar lo peligroso)? ¿Por qué?

---

### Ejercicio 3: dotenv parser

Implementa un parser mínimo de archivos `.env` que cargue variables de
entorno desde un archivo:

```
# .env file format
DATABASE_URL=postgres://localhost/mydb
APP_PORT=9090
# Comments start with #
APP_NAME=my-app
DEBUG=true

# Empty lines are ignored
SECRET_KEY="value with spaces"
```

```c
int load_dotenv(const char *path, int overwrite);
```

Requisitos:
1. Ignorar líneas vacías y líneas que empiezan con `#`.
2. Parsear `KEY=value` (el primer `=` separa nombre de valor).
3. Si `value` está entre comillas dobles, quitarlas.
4. Respetar el parámetro `overwrite` (si es 0, no sobrescribir variables
   que ya existan).
5. Retornar el número de variables cargadas, o -1 en error.
6. No usar regex ni bibliotecas externas — solo C estándar.

**Predicción antes de codificar**: ¿Qué pasa con la línea `KEY=val=ue`?
¿El valor debería ser `"val=ue"` o `"val"`?

**Pregunta de reflexión**: Herramientas como Docker y docker-compose
cargan archivos `.env` automáticamente. ¿Por qué este patrón es tan
popular, y qué riesgo existe si el archivo `.env` se sube al repositorio
git por error?
