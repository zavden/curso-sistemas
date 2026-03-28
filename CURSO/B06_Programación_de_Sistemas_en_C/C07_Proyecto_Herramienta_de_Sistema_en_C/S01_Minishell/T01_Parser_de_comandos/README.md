# Parser de comandos

## Índice
1. [Qué hace una shell](#1-qué-hace-una-shell)
2. [El loop REPL](#2-el-loop-repl)
3. [Leer la entrada: readline vs getline](#3-leer-la-entrada-readline-vs-getline)
4. [Tokenización](#4-tokenización)
5. [Manejo de comillas](#5-manejo-de-comillas)
6. [Detectar operadores especiales](#6-detectar-operadores-especiales)
7. [Estructura de datos del comando](#7-estructura-de-datos-del-comando)
8. [Parser: de tokens a comando](#8-parser-de-tokens-a-comando)
9. [Parser de pipes](#9-parser-de-pipes)
10. [Parser de redirecciones](#10-parser-de-redirecciones)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Qué hace una shell

Una shell es un programa que **lee comandos**, los **parsea**, y
los **ejecuta**. En este capítulo construiremos una mini-shell
paso a paso. Este primer tópico cubre las fases de lectura y
parseo:

```
Entrada del usuario:
  "ls -la /tmp | grep test > output.txt"

          │
          ▼
  ┌───────────────┐
  │   1. Leer     │  ← readline / getline
  └───────┬───────┘
          ▼
  ┌───────────────┐
  │  2. Tokenizar │  ← dividir en tokens
  │               │    ["ls", "-la", "/tmp", "|", "grep",
  └───────┬───────┘     "test", ">", "output.txt"]
          ▼
  ┌───────────────┐
  │  3. Parsear   │  ← construir estructura
  │               │    Pipeline:
  └───────┬───────┘      cmd[0]: ls -la /tmp
          │                cmd[1]: grep test  > output.txt
          ▼
  ┌───────────────┐
  │  4. Ejecutar  │  ← fork + exec (próximo tópico)
  └───────────────┘
```

### Anatomía de un comando de shell

```
  ls    -la     /tmp   |   grep   test   >   output.txt   &
  ^^    ^^^     ^^^^   ^   ^^^^   ^^^^   ^   ^^^^^^^^^^   ^
  │      │       │     │    │      │     │      │         │
comando args    args  pipe  cmd   args  redir  archivo   background
```

Elementos que nuestro parser debe reconocer:

| Elemento | Ejemplos |
|----------|----------|
| Palabra (comando/argumento) | `ls`, `-la`, `/tmp`, `test` |
| Pipe | `\|` |
| Redirección de salida | `>`, `>>` |
| Redirección de entrada | `<` |
| Background | `&` |
| Comillas simples | `'hello world'` |
| Comillas dobles | `"hello world"` |

---

## 2. El loop REPL

REPL = Read-Eval-Print-Loop. Es el corazón de toda shell
interactiva:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 4096

int main(void) {
    char line[MAX_LINE];

    for (;;) {
        // 1. Prompt
        printf("minish> ");
        fflush(stdout);

        // 2. Read
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;  // EOF (Ctrl+D)

        // Quitar el \n final
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';

        // Ignorar líneas vacías
        if (line[0] == '\0')
            continue;

        // 3. Tokenize + Parse
        // ... (lo que vamos a construir)

        // 4. Execute
        // ... (próximo tópico)
    }

    printf("\nexit\n");
    return 0;
}
```

### Prompt con información útil

```c
void print_prompt(void) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        // Acortar el home directory con ~
        char *home = getenv("HOME");
        if (home && strncmp(cwd, home, strlen(home)) == 0) {
            printf("minish:~%s> ", cwd + strlen(home));
        } else {
            printf("minish:%s> ", cwd);
        }
    } else {
        printf("minish> ");
    }
    fflush(stdout);
}
```

---

## 3. Leer la entrada: readline vs getline

### fgets (simple, sin dependencias)

```c
char line[4096];
if (fgets(line, sizeof(line), stdin) == NULL) {
    // EOF o error
}
```

Limitaciones: sin edición de línea (no puedes usar flechas para
navegar el historial ni mover el cursor).

### getline (POSIX, buffer dinámico)

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

char *line = NULL;
size_t bufsize = 0;

ssize_t n = getline(&line, &bufsize, stdin);
if (n == -1) {
    // EOF o error
}
// line contiene la línea leída (incluyendo \n)
// getline maneja realloc si la línea es larga

// Quitar \n
if (n > 0 && line[n - 1] == '\n')
    line[n - 1] = '\0';

free(line);  // liberar cuando termines
```

### readline (GNU, con edición y historial)

```c
#include <readline/readline.h>
#include <readline/history.h>

char *line = readline("minish> ");
if (line == NULL) {
    // EOF (Ctrl+D)
    break;
}

if (line[0] != '\0') {
    add_history(line);  // guardar en historial (flecha arriba)
}

// Usar line...
free(line);  // readline usa malloc
```

Compilar con `-lreadline`:
```bash
gcc -o minish minish.c -lreadline
```

readline proporciona:
- Edición tipo emacs (Ctrl+A, Ctrl+E, Ctrl+K...)
- Historial con flechas arriba/abajo
- Tab completion (configurable)
- Búsqueda con Ctrl+R

> **Para nuestra mini-shell**: usaremos `getline` para mantener la
> dependencia mínima. Puedes cambiar a readline después.

---

## 4. Tokenización

Tokenizar es dividir la línea en unidades significativas (tokens).
No podemos simplemente usar `strtok(line, " ")` porque:

1. Las comillas agrupan palabras: `echo "hello world"` → 2 tokens
2. Los operadores son tokens aunque estén pegados: `ls|grep` → 3 tokens
3. Las comillas pueden contener espacios que no son separadores

### Tokenizador básico (sin comillas)

Empecemos con una versión que solo maneja espacios como separadores:

```c
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 256

// Tokenizador simple: divide por espacios
int tokenize_simple(char *line, char **tokens) {
    int count = 0;
    char *tok = strtok(line, " \t");

    while (tok != NULL && count < MAX_TOKENS - 1) {
        tokens[count++] = tok;
        tok = strtok(NULL, " \t");
    }
    tokens[count] = NULL;  // terminar con NULL (para execvp)
    return count;
}
```

Esto funciona para `ls -la /tmp` pero falla con comillas y
operadores pegados.

### Tokenizador completo

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_TOKENS   256
#define MAX_TOKEN_LEN 4096

typedef struct {
    char  *tokens[MAX_TOKENS];
    int    count;
    char   storage[MAX_TOKENS * MAX_TOKEN_LEN]; // pool de memoria
    size_t storage_used;
} token_list_t;

// Reservar espacio para un token en el pool
static char *alloc_token(token_list_t *tl, const char *src, size_t len) {
    if (tl->storage_used + len + 1 > sizeof(tl->storage))
        return NULL;
    char *dest = tl->storage + tl->storage_used;
    memcpy(dest, src, len);
    dest[len] = '\0';
    tl->storage_used += len + 1;
    return dest;
}

// ¿Es un carácter operador?
static bool is_operator_char(char c) {
    return c == '|' || c == '<' || c == '>' || c == '&' || c == ';';
}

int tokenize(const char *line, token_list_t *tl) {
    tl->count = 0;
    tl->storage_used = 0;

    const char *p = line;

    while (*p != '\0') {
        // Saltar espacios
        while (*p != '\0' && isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break;

        char token_buf[MAX_TOKEN_LEN];
        int  token_len = 0;

        // Caso 1: operador (|, <, >, >>, &, ;)
        if (is_operator_char(*p)) {
            token_buf[token_len++] = *p;
            // >> (append)
            if (*p == '>' && *(p + 1) == '>') {
                p++;
                token_buf[token_len++] = *p;
            }
            p++;

            char *tok = alloc_token(tl, token_buf, token_len);
            if (tok == NULL) return -1;
            tl->tokens[tl->count++] = tok;
            continue;
        }

        // Caso 2: palabra (posiblemente con comillas)
        while (*p != '\0' && !isspace((unsigned char)*p)
               && !is_operator_char(*p)) {

            if (*p == '\'') {
                // Comilla simple: todo literal hasta la siguiente '
                p++;  // saltar la comilla de apertura
                while (*p != '\0' && *p != '\'') {
                    if (token_len < MAX_TOKEN_LEN - 1)
                        token_buf[token_len++] = *p;
                    p++;
                }
                if (*p == '\'') p++;  // saltar la de cierre
            } else if (*p == '"') {
                // Comilla doble: todo literal excepto \ y "
                p++;
                while (*p != '\0' && *p != '"') {
                    if (*p == '\\' && *(p + 1) != '\0') {
                        p++;  // saltar el backslash
                    }
                    if (token_len < MAX_TOKEN_LEN - 1)
                        token_buf[token_len++] = *p;
                    p++;
                }
                if (*p == '"') p++;
            } else if (*p == '\\') {
                // Escape: el siguiente carácter es literal
                p++;
                if (*p != '\0') {
                    if (token_len < MAX_TOKEN_LEN - 1)
                        token_buf[token_len++] = *p;
                    p++;
                }
            } else {
                // Carácter normal
                if (token_len < MAX_TOKEN_LEN - 1)
                    token_buf[token_len++] = *p;
                p++;
            }
        }

        if (token_len > 0) {
            char *tok = alloc_token(tl, token_buf, token_len);
            if (tok == NULL) return -1;
            tl->tokens[tl->count++] = tok;
        }
    }

    tl->tokens[tl->count] = NULL;
    return tl->count;
}
```

### Ejemplos de tokenización

```
Entrada: ls -la /tmp
Tokens:  ["ls", "-la", "/tmp"]

Entrada: echo "hello world"
Tokens:  ["echo", "hello world"]

Entrada: ls|grep test>out.txt
Tokens:  ["ls", "|", "grep", "test", ">", "out.txt"]

Entrada: echo 'it'\''s fine'
Tokens:  ["echo", "it's fine"]

Entrada: cat << EOF
Tokens:  ["cat", "<", "<", "EOF"]   (o tratar << como operador)
```

---

## 5. Manejo de comillas

Las shells manejan tres tipos de quoting:

### Comillas simples (`'...'`)

Todo es literal. No hay escape dentro de comillas simples:

```bash
echo 'hello $HOME \n "quotes"'
# Salida: hello $HOME \n "quotes"
```

```c
// En el tokenizador: copiar todo hasta la siguiente '
case '\'':
    p++;
    while (*p && *p != '\'')
        buf[len++] = *p++;
    if (*p == '\'') p++;
    break;
```

### Comillas dobles (`"..."`)

Casi todo es literal, pero `\` funciona como escape para `"`, `\`,
`$`, y `` ` ``:

```bash
echo "hello \"world\" and \\backslash"
# Salida: hello "world" and \backslash
```

```c
// En el tokenizador: copiar todo, honrar \ como escape
case '"':
    p++;
    while (*p && *p != '"') {
        if (*p == '\\' && (p[1] == '"' || p[1] == '\\'))
            p++;  // saltar el backslash
        buf[len++] = *p++;
    }
    if (*p == '"') p++;
    break;
```

### Backslash (`\`)

Fuera de comillas, `\` escapa el siguiente carácter:

```bash
echo hello\ world    # "hello world" (espacio escapado)
echo hello\|world    # "hello|world" (pipe escapado)
```

### Comillas mixtas

Las comillas pueden estar mezcladas en un mismo token:

```bash
echo "hello "'world'   # Un solo token: hello world
echo he"llo wor"ld     # Un solo token: hello world
```

Nuestro tokenizador ya maneja esto porque no termina el token
al cambiar de tipo de comilla — solo termina con espacio o
operador.

---

## 6. Detectar operadores especiales

Los operadores que nuestra shell debe reconocer:

```
┌──────────┬───────────────────────────────────────┐
│ Operador │ Significado                           │
├──────────┼───────────────────────────────────────┤
│ |        │ Pipe: stdout de izq → stdin de der    │
│ >        │ Redir stdout a archivo (truncar)      │
│ >>       │ Redir stdout a archivo (append)       │
│ <        │ Redir stdin desde archivo             │
│ &        │ Ejecutar en background                │
│ ;        │ Separador de comandos (secuencial)    │
│ 2>       │ Redir stderr (extensión opcional)     │
│ 2>&1     │ Redir stderr a stdout (ext. opcional) │
└──────────┴───────────────────────────────────────┘
```

### Operadores vs palabras

El tokenizador debe distinguir operadores de palabras normales.
La regla es: los caracteres `|`, `<`, `>`, `&`, `;` siempre
inician un token nuevo, incluso si están pegados a palabras:

```
Entrada: echo hello>file
Tokens:  ["echo", "hello", ">", "file"]

Entrada: ls -l|wc
Tokens:  ["ls", "-l", "|", "wc"]

Entrada: cmd1;cmd2
Tokens:  ["cmd1", ";", "cmd2"]
```

### Operadores de dos caracteres

`>>` es un solo operador (append), no dos `>` separados. El
tokenizador debe mirar un carácter adelante (lookahead):

```c
if (*p == '>' && *(p + 1) == '>') {
    // Operador >>
    token = ">>";
    p += 2;
} else if (*p == '>') {
    // Operador >
    token = ">";
    p += 1;
}
```

---

## 7. Estructura de datos del comando

Después de tokenizar, necesitamos una estructura que represente
el comando parseado:

```c
#include <stdbool.h>

#define MAX_ARGS     128
#define MAX_PIPELINE 16

// Un comando simple (sin pipes)
typedef struct {
    char *argv[MAX_ARGS];   // argv[0] = comando, argv[n] = NULL
    int   argc;             // número de argumentos
    char *redir_in;         // archivo para stdin (<), NULL si no hay
    char *redir_out;        // archivo para stdout (>), NULL si no hay
    bool  append_out;       // true si es >> en vez de >
    char *redir_err;        // archivo para stderr (2>), NULL si no hay
} command_t;

// Un pipeline (secuencia de comandos conectados por |)
typedef struct {
    command_t commands[MAX_PIPELINE];
    int       num_commands;  // 1 = simple, >1 = pipeline
    bool      background;    // true si termina en &
} pipeline_t;
```

### Representación visual

```
Entrada: ls -la /tmp | grep test > output.txt &

pipeline_t:
  background: true
  num_commands: 2
  commands[0]:
    argv: ["ls", "-la", "/tmp", NULL]
    argc: 3
    redir_in:  NULL
    redir_out: NULL
  commands[1]:
    argv: ["grep", "test", NULL]
    argc: 2
    redir_in:  NULL
    redir_out: "output.txt"
    append_out: false
```

---

## 8. Parser: de tokens a comando

El parser toma la lista de tokens y construye la estructura
`pipeline_t`:

```c
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Parsear tokens en un pipeline_t
// Retorna 0 en éxito, -1 en error de sintaxis
int parse_tokens(char **tokens, int num_tokens, pipeline_t *pl) {
    memset(pl, 0, sizeof(*pl));
    pl->num_commands = 1;

    command_t *cmd = &pl->commands[0];

    for (int i = 0; i < num_tokens; i++) {
        char *tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            // Pipe: terminar comando actual, empezar uno nuevo
            if (cmd->argc == 0) {
                fprintf(stderr, "minish: syntax error near '|'\n");
                return -1;
            }
            cmd->argv[cmd->argc] = NULL;

            pl->num_commands++;
            if (pl->num_commands > MAX_PIPELINE) {
                fprintf(stderr, "minish: too many pipes\n");
                return -1;
            }
            cmd = &pl->commands[pl->num_commands - 1];

        } else if (strcmp(tok, "<") == 0) {
            // Redirección de entrada
            i++;
            if (i >= num_tokens) {
                fprintf(stderr, "minish: syntax error near '<'\n");
                return -1;
            }
            cmd->redir_in = tokens[i];

        } else if (strcmp(tok, ">") == 0) {
            // Redirección de salida (truncar)
            i++;
            if (i >= num_tokens) {
                fprintf(stderr, "minish: syntax error near '>'\n");
                return -1;
            }
            cmd->redir_out = tokens[i];
            cmd->append_out = false;

        } else if (strcmp(tok, ">>") == 0) {
            // Redirección de salida (append)
            i++;
            if (i >= num_tokens) {
                fprintf(stderr, "minish: syntax error near '>>'\n");
                return -1;
            }
            cmd->redir_out = tokens[i];
            cmd->append_out = true;

        } else if (strcmp(tok, "&") == 0) {
            // Background (debe ser el último token)
            pl->background = true;

        } else if (strcmp(tok, ";") == 0) {
            // Para nuestra mini-shell, tratamos ; como separador
            // (implementación completa requeriría parsear múltiples
            // pipelines, lo dejamos como extensión)
            break;

        } else {
            // Argumento normal
            if (cmd->argc >= MAX_ARGS - 1) {
                fprintf(stderr, "minish: too many arguments\n");
                return -1;
            }
            cmd->argv[cmd->argc++] = tok;
        }
    }

    // Terminar el último comando
    cmd->argv[cmd->argc] = NULL;

    // Verificar que el último comando no está vacío
    if (cmd->argc == 0 && pl->num_commands > 1) {
        fprintf(stderr, "minish: syntax error near end of command\n");
        return -1;
    }

    return 0;
}
```

### Probando el parser

```c
#include <stdio.h>

void print_pipeline(const pipeline_t *pl) {
    printf("Pipeline: %d command(s), background=%d\n",
           pl->num_commands, pl->background);

    for (int i = 0; i < pl->num_commands; i++) {
        const command_t *cmd = &pl->commands[i];
        printf("  cmd[%d]: ", i);
        for (int j = 0; j < cmd->argc; j++)
            printf("%s ", cmd->argv[j]);

        if (cmd->redir_in)
            printf("< %s ", cmd->redir_in);
        if (cmd->redir_out)
            printf("%s %s ", cmd->append_out ? ">>" : ">",
                   cmd->redir_out);
        printf("\n");
    }
}

int main(void) {
    // Test
    char line[] = "ls -la /tmp | grep test > output.txt &";
    token_list_t tl;
    tokenize(line, &tl);

    pipeline_t pl;
    if (parse_tokens(tl.tokens, tl.count, &pl) == 0) {
        print_pipeline(&pl);
    }

    return 0;
}
```

Salida esperada:
```
Pipeline: 2 command(s), background=1
  cmd[0]: ls -la /tmp
  cmd[1]: grep test > output.txt
```

---

## 9. Parser de pipes

Los pipes son el caso más importante del parser. Un pipeline
conecta la salida de un comando con la entrada del siguiente:

```
ls -la | grep test | wc -l

  ┌────────┐     ┌───────────┐     ┌──────┐
  │ ls -la │────▶│ grep test │────▶│ wc -l│
  │ stdout │pipe │stdin  out │pipe │stdin │
  └────────┘     └───────────┘     └──────┘
```

### Cómo el parser identifica pipes

El token `|` divide la lista de tokens en segmentos, cada uno
es un comando del pipeline:

```
Tokens: ["ls", "-la", "|", "grep", "test", "|", "wc", "-l"]

Segmento 0: ["ls", "-la"]           → commands[0]
         |
Segmento 1: ["grep", "test"]        → commands[1]
         |
Segmento 2: ["wc", "-l"]            → commands[2]

num_commands = 3
```

### Errores de sintaxis con pipes

El parser debe detectar:

```c
// Pipe al inicio
"|  ls"           → error: syntax error near '|'

// Pipe al final
"ls |"            → error: syntax error near end

// Pipes consecutivos
"ls || grep"      → error: syntax error near '|'

// Pipe sin comando antes
"| grep test"     → error: syntax error near '|'
```

Nuestro parser ya detecta estos errores:
- `cmd->argc == 0` cuando vemos `|` → pipe sin comando previo
- Último `cmd->argc == 0` con `num_commands > 1` → pipe sin
  comando después

---

## 10. Parser de redirecciones

Las redirecciones pueden aparecer en cualquier posición dentro de
un comando:

```bash
# Todas estas son equivalentes:
grep test < input.txt > output.txt
> output.txt grep test < input.txt
grep > output.txt test < input.txt
```

Nuestro parser maneja esto correctamente porque procesa los tokens
secuencialmente: los tokens `<`, `>`, `>>` consumen el siguiente
token como nombre de archivo, y los demás tokens se acumulan en
`argv`.

### Redirección de stderr (extensión)

Para soportar `2>` y `2>&1`, necesitamos reconocer estos patrones:

```c
// En el tokenizador, antes de verificar operadores genéricos:
if (*p == '2' && *(p + 1) == '>') {
    if (*(p + 2) == '&' && *(p + 3) == '1') {
        // 2>&1: redirigir stderr a stdout
        token = "2>&1";
        p += 4;
    } else {
        // 2>: redirigir stderr a archivo
        token = "2>";
        p += 2;
    }
}
```

```c
// En el parser:
} else if (strcmp(tok, "2>") == 0) {
    i++;
    if (i >= num_tokens) {
        fprintf(stderr, "minish: syntax error near '2>'\n");
        return -1;
    }
    cmd->redir_err = tokens[i];
}
```

### Múltiples redirecciones

Un comando puede tener redirección de entrada Y salida:

```bash
sort < unsorted.txt > sorted.txt
```

Y un comando en un pipeline puede tener redirecciones (la primera
y última posición son las más comunes):

```bash
cat < input.txt | grep test | sort > output.txt
```

---

## 11. Errores comunes

### Error 1: usar strtok para todo

```c
// MAL: strtok no puede manejar comillas ni operadores pegados
char *tokens[64];
int i = 0;
char *tok = strtok(line, " ");
while (tok) {
    tokens[i++] = tok;
    tok = strtok(NULL, " ");
}
// "echo hello>file" → tokens: ["echo", "hello>file"]
// Debería ser: ["echo", "hello", ">", "file"]
```

**Solución**: escribir un tokenizador que reconozca operadores y
comillas (como el que implementamos arriba).

### Error 2: no terminar argv con NULL

```c
// MAL: execvp necesita que argv termine en NULL
cmd.argv[0] = "ls";
cmd.argv[1] = "-la";
// cmd.argv[2] no es NULL → execvp lee basura del stack
```

**Solución**: siempre terminar:

```c
cmd.argv[cmd.argc] = NULL;
```

### Error 3: comillas sin cerrar

```c
// MAL: no detectar comillas sin cerrar
// echo "hello
// El tokenizador lee hasta el final del string sin encontrar "
```

**Solución**: detectar y reportar el error:

```c
if (*p == '"') {
    p++;
    while (*p && *p != '"')
        buf[len++] = *p++;
    if (*p == '\0') {
        fprintf(stderr, "minish: syntax error: unclosed quote\n");
        return -1;
    }
    p++;  // cerrar comilla
}
```

### Error 4: modificar la cadena original con strtok

```c
// MAL: strtok modifica line in-place
char line[4096];
fgets(line, sizeof(line), stdin);
strtok(line, " ");  // inserta \0 en line
// Si necesitas line completa después (para historial), ya se corrompió
```

**Solución**: trabajar con una copia o usar un tokenizador que no
modifique la entrada (como el nuestro, que copia tokens a un
storage separado).

### Error 5: no manejar la línea vacía

```c
// MAL: intentar parsear una línea vacía
// "" → tokens vacíos → argv[0] = NULL → execvp(NULL, ...) → crash
```

**Solución**: verificar antes de ejecutar:

```c
if (line[0] == '\0') continue;  // saltar líneas vacías

// Y después del parse:
if (pl.commands[0].argc == 0) continue;  // nada que ejecutar
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              Parser de Comandos — Mini-shell                 │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FLUJO:                                                      │
│    prompt → getline → tokenize → parse → execute             │
│                                                              │
│  TOKENS:                                                     │
│    Palabras:     ls, -la, /tmp, hello                        │
│    Operadores:   |  >  >>  <  &  ;                           │
│    Con comillas:  "hello world"  'hello world'               │
│    Con escape:    hello\ world   \|  \\                      │
│                                                              │
│  REGLAS DE COMILLAS:                                         │
│    '...'  todo literal, sin escape                           │
│    "..."  literal excepto \\ y \"                            │
│    \x     el carácter x literal (fuera de comillas)          │
│    Las comillas no terminan un token — se pueden mezclar     │
│                                                              │
│  ESTRUCTURAS:                                                │
│    command_t:   argv[], redir_in, redir_out, append_out      │
│    pipeline_t:  commands[], num_commands, background          │
│                                                              │
│  PARSE:                                                      │
│    Palabra → añadir a argv del comando actual                │
│    |       → terminar comando actual, empezar nuevo          │
│    > file  → redir_out = file, append_out = false            │
│    >> file → redir_out = file, append_out = true             │
│    < file  → redir_in = file                                 │
│    &       → background = true                               │
│                                                              │
│  ERRORES DE SINTAXIS:                                        │
│    Pipe sin comando antes/después: "| ls", "ls |"            │
│    Redirección sin archivo: "ls >", "cat <"                  │
│    Comillas sin cerrar: echo "hello                          │
│    Demasiados argumentos o pipes                             │
│                                                              │
│  COMPILAR:                                                   │
│    gcc -o minish minish.c                                    │
│    gcc -o minish minish.c -lreadline  (con GNU readline)     │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: tokenizador interactivo

Implementa un programa que funcione como tokenizador de debug:

1. Leer líneas del stdin en un loop.
2. Tokenizar cada línea con el tokenizador completo.
3. Imprimir cada token numerado con su tipo:

```
minish-tok> echo "hello world" | grep hello > out.txt &
Token 0: [WORD]     "echo"
Token 1: [WORD]     "hello world"
Token 2: [PIPE]     "|"
Token 3: [WORD]     "grep"
Token 4: [WORD]     "hello"
Token 5: [REDIR_OUT]">"
Token 6: [WORD]     "out.txt"
Token 7: [BG]       "&"
```

Probar con estos casos:
- `echo 'hello "world"'` → `hello "world"` (comillas simples)
- `echo "it's"` → `it's` (comillas dobles)
- `ls|grep test>out` → 5 tokens (operadores pegados)
- `echo hello\ world` → `hello world` (escape de espacio)
- `echo "unclosed` → error de sintaxis

**Pregunta de reflexión**: bash tiene más tipos de quoting (ANSI-C
quotes `$'...'`, heredocs `<<EOF`). ¿Cuánto más complejo sería
el tokenizador si soportara `$HOME` (expansión de variables)
dentro de comillas dobles?

---

### Ejercicio 2: parser con pretty-print

Implementa el parser completo y un pretty-printer que muestre la
estructura parseada:

1. Tokenizar + parsear la entrada.
2. Para cada pipeline, imprimir:

```
minish-parse> cat < input.txt | sort -r | head -5 > output.txt &

Pipeline (3 commands, background=yes):
  [0] cat
      stdin:  input.txt
  [1] sort -r
      stdin:  pipe
      stdout: pipe
  [2] head -5
      stdin:  pipe
      stdout: output.txt (truncate)
```

3. Detectar y reportar todos los errores de sintaxis:
   - `| ls` → error
   - `ls >` → error
   - `ls > > file` → error (doble redirección)
   - `ls | | grep` → error (pipes consecutivos)

**Pregunta de reflexión**: en bash, `ls > file1 > file2` no es un
error — simplemente la segunda redirección gana (file1 se crea
vacío). ¿Debería nuestra mini-shell aceptar o rechazar múltiples
redirecciones del mismo tipo?

---

### Ejercicio 3: REPL con historial manual

Implementa un REPL completo sin usar readline:

1. Prompt que muestre el directorio actual (acortando `$HOME` a `~`).
2. Leer con `getline`.
3. Tokenizar y parsear (imprimir la estructura como debug).
4. Implementar un **historial manual**:
   - Guardar las últimas 100 líneas en un array circular.
   - Comando built-in `history` que imprima el historial numerado.
   - Comando `!N` que re-ejecute la línea N del historial.
   - Comando `!!` que re-ejecute la última línea.
5. Al salir (EOF o `exit`), guardar el historial en
   `~/.minish_history`. Al iniciar, cargar el historial.

```
minish:~> ls -la
  (debug: pipeline 1 cmd, argv=["ls","-la"])
minish:~> echo hello
  (debug: pipeline 1 cmd, argv=["echo","hello"])
minish:~> history
  1  ls -la
  2  echo hello
  3  history
minish:~> !1
  (re-execute: ls -la)
```

**Pregunta de reflexión**: ¿por qué las shells reales (bash, zsh)
guardan el historial en un archivo? ¿Qué pasa si abres dos
terminales simultáneamente — cómo se fusionan sus historiales?
(Bash usa append; zsh tiene la opción `SHARE_HISTORY`.)
