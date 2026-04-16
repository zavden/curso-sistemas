# Paquetes — convenciones de nombrado, visibilidad (mayuscula/minuscula), internal packages

## 1. Introduccion

El sistema de paquetes de Go es deliberadamente simple comparado con otros lenguajes. No hay headers, no hay includes con rutas relativas, no hay namespaces anidados, no hay `public`/`private`/`protected` keywords. En Go, un paquete es un **directorio** que contiene archivos `.go` con la misma declaracion `package`. La visibilidad se controla con **una sola regla**: si el identificador empieza con mayuscula, es exportado (publico); si empieza con minuscula, no es exportado (privado al paquete). Eso es todo.

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                  SISTEMA DE PAQUETES EN GO                                  │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  PAQUETE = DIRECTORIO                                                       │
│  ├─ Todos los archivos .go en un directorio comparten el mismo package     │
│  ├─ El nombre del package NO tiene que coincidir con el directorio          │
│  │   (pero por convencion casi siempre coincide)                            │
│  └─ Excepciones: package main, _test                                       │
│                                                                              │
│  VISIBILIDAD = UNA REGLA                                                    │
│  ├─ Mayuscula inicial → exportado (visible desde otros paquetes)           │
│  │   Println, Error, HandleFunc, Context, String                            │
│  └─ Minuscula inicial → no exportado (solo visible dentro del paquete)     │
│      println, errorf, handleFunc, context, string                           │
│                                                                              │
│  IMPORTS = RUTAS DE MODULO                                                  │
│  ├─ "fmt"                        → stdlib                                  │
│  ├─ "net/http"                   → stdlib, subpaquete                      │
│  ├─ "github.com/user/repo/pkg"  → dependencia externa                     │
│  └─ "./internal/auth"           → NO VALIDO (no se usan rutas relativas) │
│                                                                              │
│  HERRAMIENTAS ESPECIALES                                                    │
│  ├─ internal/ → visibilidad restringida al modulo padre                    │
│  ├─ _test.go → archivos de test, pueden usar package X_test               │
│  └─ init()   → funcion de inicializacion, ejecuta al importar             │
│                                                                              │
│  C / RUST EQUIVALENCIA                                                      │
│  ├─ C: #include "header.h" + static (file scope) + extern (global)        │
│  ├─ Rust: mod + pub/pub(crate)/pub(super) + use + crate::                 │
│  └─ Go: package + mayuscula/minuscula + import + module path              │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Declaracion de paquete

### 2.1 La clausula `package`

Todo archivo `.go` empieza con una declaracion `package`. Todos los archivos en el mismo directorio **deben** tener la misma declaracion de paquete (excepto archivos `_test.go` que pueden usar un package name con sufijo `_test`).

```go
// archivo: myproject/calculator/calculator.go
package calculator // Todos los archivos en calculator/ deben decir "package calculator"

func Add(a, b int) int {
    return a + b
}

func subtract(a, b int) int { // No exportado — solo visible dentro de calculator
    return a - b
}
```

```go
// archivo: myproject/calculator/advanced.go
package calculator // Mismo package — OBLIGATORIO, no opcional

// Esta funcion puede usar subtract() porque esta en el mismo package
func AddAndSubtract(a, b int) (int, int) {
    return Add(a, b), subtract(a, b) // Ambas accesibles
}
```

### 2.2 Nombre del package vs nombre del directorio

Por **convencion**, el nombre del package coincide con el nombre del directorio. Pero el compilador no lo exige — son cosas distintas:

```
directorio: myproject/utils/helpers/
archivo: helpers.go con "package helpers"      ← convencion normal

directorio: myproject/utils/helpers/
archivo: helpers.go con "package tools"        ← VALIDO pero confuso
                                                  import "myproject/utils/helpers"
                                                  pero usas: tools.DoSomething()
```

```go
// El import path es la ruta del DIRECTORIO, pero usas el NOMBRE DEL PACKAGE
import "myproject/utils/helpers"

// Si el package se llama "tools" (no "helpers"):
func main() {
    tools.DoSomething() // Usas el nombre del package, no el directorio
}
```

**Regla practica**: siempre haz que coincidan. La unica excepcion comun es `package main`.

### 2.3 Package main

`package main` es especial — define un programa ejecutable. Debe contener una funcion `main()`:

```go
// archivo: myproject/cmd/server/main.go
package main

import "fmt"

func main() {
    fmt.Println("Server starting...")
}
```

Un modulo puede tener multiples `package main` en diferentes directorios:

```
myproject/
├── go.mod
├── cmd/
│   ├── server/
│   │   └── main.go     // package main → go build ./cmd/server
│   ├── worker/
│   │   └── main.go     // package main → go build ./cmd/worker
│   └── cli/
│       └── main.go     // package main → go build ./cmd/cli
├── internal/
│   └── ...
└── pkg/
    └── ...
```

```bash
# Compilar cada ejecutable por separado
go build -o bin/server ./cmd/server
go build -o bin/worker ./cmd/worker
go build -o bin/cli    ./cmd/cli

# O todos a la vez
go build ./cmd/...
```

---

## 3. Convenciones de nombrado de paquetes

Go tiene convenciones fuertes sobre como nombrar paquetes. No son verificadas por el compilador, pero son practicamente universales en el ecosistema.

### 3.1 Reglas fundamentales

```
REGLAS DE NOMBRADO DE PAQUETES

1. MINUSCULAS, sin guiones bajos ni mixedCaps
   ✓ strconv, httputil, io, bufio
   ✗ strConv, http_util, IO, BufIO

2. CORTOS y concisos — una sola palabra si es posible
   ✓ fmt, os, net, sync, bytes, sort, math
   ✗ formatting, operatingsystem, networking, synchronization

3. SUSTANTIVOS, no verbos
   ✓ bytes, errors, strings, encoding
   ✗ format, compute, parse (excepciones: sort, log, test)

4. NO usar nombres genericos
   ✗ util, common, shared, helpers, misc, base
   ✓ Nombre que describe QUE contiene, no que ES

5. SIN repeticion con el contenido exportado
   ✗ http.HTTPServer → http.Server
   ✗ json.JSONMarshal → json.Marshal
   ✗ bytes.ByteBuffer → bytes.Buffer
   ✓ El nombre del package ya da contexto

6. SINGULAR, no plural (por convencion)
   ✓ string → strings (excepcion: el package trata con multiples strings)
   ✓ byte → bytes
   ✗ errors → Go lo hace, pero es la excepcion
```

### 3.2 La regla de no repeticion (la mas importante)

El nombre del package es parte de como se usa. Cuando alguien importa tu package, escribe `package.Symbol`. Si el nombre del package y el simbolo repiten informacion, el codigo se vuelve redundante:

```go
// MAL: repeticion
http.HTTPServer      // "HTTP" aparece dos veces
json.JSONDecoder     // "JSON" aparece dos veces
bytes.ByteBuffer     // "byte" aparece dos veces
context.ContextKey   // "context" aparece dos veces
log.LogEntry         // "log" aparece dos veces

// BIEN: el package ya da contexto
http.Server          // Es un server HTTP — se entiende por el package
json.Decoder         // Es un decoder JSON
bytes.Buffer         // Es un buffer de bytes
context.Key          // (hipotetico) Una key de context
log.Entry            // Una entry de log
```

```go
// Ejemplo practico: nombrar tipos en tu package

// PACKAGE: auth
package auth

// MAL
type AuthService struct { ... }     // auth.AuthService → redundante
type AuthToken struct { ... }       // auth.AuthToken → redundante
func AuthenticateUser() { ... }     // auth.AuthenticateUser → redundante

// BIEN
type Service struct { ... }         // auth.Service → claro
type Token struct { ... }           // auth.Token → claro
func Authenticate() { ... }        // auth.Authenticate → claro
```

### 3.3 Packages de la stdlib como modelo

La stdlib de Go es el mejor ejemplo de buen naming:

```go
// Observa como los nombres son cortos, descriptivos, y no repiten

import "fmt"       // fmt.Println, fmt.Sprintf, fmt.Fprintf
import "os"        // os.Open, os.Create, os.Args, os.Getenv
import "io"        // io.Reader, io.Writer, io.Copy, io.ReadAll
import "net"       // net.Dial, net.Listen, net.Conn
import "net/http"  // http.Get, http.Server, http.Handler
import "sync"      // sync.Mutex, sync.WaitGroup, sync.Once
import "context"   // context.Context, context.WithCancel
import "errors"    // errors.New, errors.Is, errors.As
import "strings"   // strings.Contains, strings.Split, strings.Builder
import "strconv"   // strconv.Atoi, strconv.Itoa, strconv.ParseFloat
import "filepath"  // filepath.Join, filepath.Walk, filepath.Ext
import "encoding/json"  // json.Marshal, json.Unmarshal, json.Decoder
import "encoding/xml"   // xml.Marshal, xml.Unmarshal
import "database/sql"   // sql.DB, sql.Row, sql.Tx
import "testing"         // testing.T, testing.B, testing.M
```

### 3.4 Nombres de archivos dentro de un package

Los archivos `.go` dentro de un package no afectan la API — puedes organizar el codigo en tantos archivos como quieras:

```
strings/
├── builder.go      // strings.Builder
├── compare.go      // strings.Compare
├── reader.go       // strings.Reader
├── replace.go      // strings.Replacer, strings.NewReplacer
├── search.go       // strings.Contains, strings.Index
├── strings.go      // strings.Split, strings.Join, etc.
└── strings_test.go // Tests
```

**Convenciones de nombres de archivos**:

```
NOMBRES DE ARCHIVOS

1. Minusculas con guiones bajos (snake_case)
   ✓ http_server.go, string_utils.go
   ✗ httpServer.go, StringUtils.go

2. Archivo principal: mismo nombre que el package
   ✓ calculator/calculator.go  (funciones principales)
   ✓ auth/auth.go              (entry point del package)

3. Archivos con sufijo especial:
   ├─ _test.go     → archivos de test (excluidos del binario final)
   ├─ _linux.go    → build constraint: solo compila en Linux
   ├─ _amd64.go    → build constraint: solo compila en amd64
   ├─ _windows.go  → build constraint: solo compila en Windows
   └─ _darwin.go   → build constraint: solo compila en macOS

4. Archivos con prefijo especial:
   └─ doc.go       → documentacion del package (solo comentarios)
```

---

## 4. Visibilidad: exportado vs no exportado

### 4.1 La regla de la mayuscula

Go usa **una sola regla** para toda la visibilidad: la primera letra determina si un identificador es exportado (visible fuera del paquete) o no exportado (solo visible dentro del paquete).

```go
package user

// EXPORTADOS (primera letra mayuscula) — visibles desde otros paquetes
type User struct { ... }
type Admin struct { ... }
func NewUser(name string) *User { ... }
func ParseID(s string) (ID, error) { ... }
var DefaultTimeout = 30 * time.Second
const MaxRetries = 3
type ID = int64

// NO EXPORTADOS (primera letra minuscula) — solo visibles dentro de "user"
type validator struct { ... }
type role int
func validate(u *User) error { ... }
func hashPassword(pw string) string { ... }
var defaultRole = role(0)
const maxPasswordLength = 128
type internalState struct { ... }
```

La regla aplica a **todo** identificador: tipos, funciones, metodos, constantes, variables, campos de struct, metodos de interface.

### 4.2 Campos de struct: visibilidad granular

Los campos de un struct siguen la misma regla — cada campo tiene su propia visibilidad:

```go
package user

type User struct {
    ID        int64     // Exportado — otros paquetes pueden leer/escribir
    Name      string    // Exportado
    Email     string    // Exportado
    password  string    // NO exportado — solo accesible dentro de "user"
    createdAt time.Time // NO exportado
    role      role      // NO exportado
}

// Constructor — la forma idiomatica de crear structs con campos no exportados
func NewUser(name, email, password string) *User {
    return &User{
        ID:        generateID(),
        Name:      name,
        Email:     email,
        password:  hashPassword(password),
        createdAt: time.Now(),
        role:      defaultRole,
    }
}

// Getters para campos no exportados (cuando necesitas leerlos desde fuera)
func (u *User) CreatedAt() time.Time {
    return u.createdAt
}

// No hay setter generico — cada campo que necesite escritura externa
// tiene un metodo con validacion
func (u *User) SetPassword(newPassword string) error {
    if len(newPassword) < 8 {
        return errors.New("password too short")
    }
    u.password = hashPassword(newPassword)
    return nil
}
```

```go
// Desde otro paquete:
package main

import "myproject/user"

func main() {
    u := user.NewUser("Alice", "alice@example.com", "secret123")
    
    fmt.Println(u.Name)       // OK — Name es exportado
    fmt.Println(u.Email)      // OK — Email es exportado
    fmt.Println(u.CreatedAt()) // OK — metodo exportado
    
    // fmt.Println(u.password)  // ERROR: u.password undefined (unexported field)
    // fmt.Println(u.createdAt) // ERROR: u.createdAt undefined (unexported field)
    // u.password = "hack"      // ERROR
    
    u.SetPassword("newpassword") // OK — metodo exportado con validacion
}
```

### 4.3 Interfaces: metodos exportados y no exportados

```go
package storage

// Interface exportada con metodos exportados
type Store interface {
    Get(key string) ([]byte, error)
    Set(key string, value []byte) error
    Delete(key string) error
}

// Interface exportada con mezcla — raro pero valido
type AdvancedStore interface {
    Store              // Todos los metodos de Store (exportados)
    compact() error    // No exportado — solo implementable DENTRO de "storage"
    //                    Esto hace que tipos fuera del package NO puedan
    //                    implementar AdvancedStore, porque no pueden
    //                    definir el metodo compact() con este nombre.
}

// Esto es un PATRON para "sellar" una interface:
// Cualquiera puede USAR AdvancedStore como parametro/retorno,
// pero solo el package "storage" puede IMPLEMENTARLA.
```

```go
// Interface sellada — patron comun en la stdlib
package ast

// Node es implementada por todos los nodos del AST.
// El metodo no exportado impide implementaciones externas.
type Node interface {
    Pos() token.Pos   // Exportado — cualquiera puede llamarlo
    End() token.Pos   // Exportado
    astNode()          // NO exportado — "sella" la interface
}

// Solo los tipos dentro de "ast" pueden implementar Node:
type Ident struct { ... }
func (i *Ident) Pos() token.Pos { ... }
func (i *Ident) End() token.Pos { ... }
func (i *Ident) astNode() {}  // Puede implementar el metodo privado
```

### 4.4 Metodos exportados vs no exportados

```go
package account

type Account struct {
    balance int64
    owner   string
}

// Metodo EXPORTADO — parte de la API publica
func (a *Account) Balance() int64 {
    return a.balance
}

// Metodo EXPORTADO — con validacion
func (a *Account) Deposit(amount int64) error {
    if amount <= 0 {
        return errors.New("amount must be positive")
    }
    a.balance += amount
    a.logTransaction("deposit", amount)
    return nil
}

// Metodo EXPORTADO
func (a *Account) Withdraw(amount int64) error {
    if amount <= 0 {
        return errors.New("amount must be positive")
    }
    if !a.hasSufficientFunds(amount) {
        return errors.New("insufficient funds")
    }
    a.balance -= amount
    a.logTransaction("withdrawal", amount)
    return nil
}

// Metodo NO EXPORTADO — detalle de implementacion
func (a *Account) hasSufficientFunds(amount int64) bool {
    return a.balance >= amount
}

// Metodo NO EXPORTADO — detalle de implementacion
func (a *Account) logTransaction(txType string, amount int64) {
    log.Printf("[%s] %s: %d (balance: %d)", a.owner, txType, amount, a.balance)
}
```

### 4.5 Visibilidad y embedding

Cuando un tipo esta embebido en un struct, los campos y metodos exportados del tipo embebido se "promueven" — son accesibles directamente:

```go
package server

type Logger struct {
    Level string // Exportado
    path  string // No exportado
}

func (l *Logger) Info(msg string) {
    fmt.Printf("[%s] INFO: %s\n", l.Level, msg)
}

func (l *Logger) debug(msg string) { // No exportado
    fmt.Printf("[%s] DEBUG: %s\n", l.Level, msg)
}

type Server struct {
    Logger           // Embebido — promueve campos/metodos exportados
    Addr   string
    port   int
}
```

```go
// Desde otro paquete:
package main

import "myproject/server"

func main() {
    s := server.Server{
        Logger: server.Logger{Level: "prod"},
        Addr:   "0.0.0.0",
    }
    
    s.Info("starting")     // OK — promovido desde Logger (exportado)
    fmt.Println(s.Level)   // OK — campo exportado de Logger promovido
    
    // s.debug("test")     // ERROR — metodo no exportado no se promueve
    // fmt.Println(s.path) // ERROR — campo no exportado no se promueve
    // s.port = 8080       // ERROR — campo no exportado de Server
}
```

### 4.6 Comparacion de visibilidad: Go vs C vs Rust

```
VISIBILIDAD: GO vs C vs RUST

Go:
  ├─ Mayuscula = exportado (visible fuera del package)
  ├─ Minuscula = no exportado (solo dentro del package)
  └─ No hay granularidad intermedia (no hay "friend", "protected", etc.)
      Excepcion: internal/ (ver seccion 6)

C:
  ├─ extern = visible en todas las translation units (default para funciones)
  ├─ static = solo visible en el archivo actual (file scope)
  ├─ Headers (.h) controlan QUE se expone al usuario
  └─ No hay "paquetes" — el programador organiza manualmente

Rust:
  ├─ pub         = publico (visible desde cualquier lugar)
  ├─ pub(crate)  = visible dentro del crate actual
  ├─ pub(super)  = visible en el modulo padre
  ├─ pub(in path)= visible en un path especifico
  └─ (nada)      = privado al modulo actual
      Rust tiene el sistema mas granular de los tres
```

```
┌──────────────────────────┬──────────────┬──────────────┬───────────────────┐
│ Concepto                  │ Go           │ C            │ Rust              │
├──────────────────────────┼──────────────┼──────────────┼───────────────────┤
│ Unidad de visibilidad     │ Package      │ Archivo (.c) │ Modulo            │
│ Publico                   │ Mayuscula    │ extern/header│ pub               │
│ Privado                   │ Minuscula    │ static       │ (default)         │
│ Nivel intermedio           │ internal/    │ No           │ pub(crate), etc.  │
│ Keyword                   │ Ninguna      │ static/extern│ pub               │
│ Enforcement               │ Compilador   │ Linker       │ Compilador        │
│ Granularidad por campo    │ Si           │ No*          │ Si                │
│ Interface sealing         │ metodo priv. │ N/A          │ sealed trait      │
└──────────────────────────┴──────────────┴──────────────┴───────────────────┘
*En C, un struct expuesto en un header tiene todos sus campos visibles.
 Se puede usar "opaque pointers" para esconderlos.
```

---

## 5. Imports

### 5.1 Sintaxis basica

```go
// Import individual
import "fmt"

// Import multiple (agrupado) — la forma mas comun
import (
    "fmt"
    "os"
    "strings"
)

// Convencion de agrupacion (goimports lo hace automaticamente):
import (
    // Grupo 1: stdlib
    "context"
    "fmt"
    "net/http"
    "os"
    "time"

    // Grupo 2: dependencias externas (separadas por linea en blanco)
    "github.com/gorilla/mux"
    "go.uber.org/zap"

    // Grupo 3: paquetes internos del proyecto (separados por linea en blanco)
    "myproject/internal/auth"
    "myproject/internal/database"
    "myproject/pkg/models"
)
```

### 5.2 Import con alias

```go
import (
    "fmt"
    
    // Alias: renombrar el package para evitar conflictos o acortar
    crand "crypto/rand"   // Usar como crand.Read()
    mrand "math/rand"     // Usar como mrand.Intn()
    
    // Ambos se llaman "rand" sin alias — conflicto de nombre
)

func main() {
    // Sin alias, "rand" seria ambiguo
    b := make([]byte, 16)
    crand.Read(b)           // crypto/rand
    n := mrand.Intn(100)    // math/rand
    fmt.Println(b, n)
}
```

### 5.3 Dot import (generalmente no recomendado)

```go
import . "fmt"

func main() {
    Println("sin prefijo") // Equivale a fmt.Println
    Sprintf("sin %s", "prefijo")
}

// CUANDO es aceptable:
// - En archivos de test, para DSLs de testing:
//   import . "github.com/onsi/gomega"
//   Expect(result).To(Equal(42))
//
// CUANDO NO usarlo:
// - En codigo de produccion — hace confuso de donde viene cada simbolo
```

### 5.4 Blank import (import por side-effects)

```go
import (
    "database/sql"
    
    _ "github.com/lib/pq" // Solo ejecuta init() — registra el driver PostgreSQL
    // No usamos ningun simbolo de pq directamente
    // Pero su init() llama a sql.Register("postgres", &Driver{})
)

func main() {
    db, err := sql.Open("postgres", "postgres://localhost/mydb")
    // El driver "postgres" esta disponible gracias al blank import
}
```

```go
// Otro uso comun: asegurar que un package se importa para verificar
// que implementa una interface en tiempo de compilacion

import _ "myproject/plugins/redis" // Su init() registra el plugin
```

### 5.5 Import circular: prohibido

Go **no permite** imports circulares. Si el paquete A importa B y B importa A, el compilador falla:

```
ERROR: import cycle not allowed
  package A imports package B
  package B imports package A
```

```
SOLUCION A IMPORTS CIRCULARES

Situacion:
  user/ → necesita algo de order/
  order/ → necesita algo de user/

Soluciones:
  1. EXTRAER: mover lo compartido a un tercer paquete
     user/ → types/
     order/ → types/
  
  2. INTERFACES: definir la interface en el paquete que la necesita
     order/ define: type UserFetcher interface { GetUser(id int) User }
     user/ implementa UserFetcher
     main/ conecta ambos (dependency injection)
  
  3. REORGANIZAR: quiza user y order deberian ser un solo paquete
     (Go prefiere paquetes mas grandes a paquetes tiny)
  
  4. INVERTIR DEPENDENCIA: el paquete de nivel bajo no deberia
     conocer al de nivel alto. Usa interfaces para invertir.
```

```go
// Ejemplo: resolviendo ciclo con interfaces

// ANTES (ciclo):
// package user
// import "myproject/order"
// func (u *User) Orders() []order.Order { ... }

// package order  
// import "myproject/user"
// func (o *Order) Owner() user.User { ... }  // CICLO!

// DESPUES (sin ciclo):
// package order
type UserInfo struct {  // order define su propia representacion del user
    ID   int64
    Name string
}

type Order struct {
    ID    int64
    Owner UserInfo  // No importa "user" — usa su propio tipo
    Items []Item
}

// package user
import "myproject/order"

type User struct {
    ID   int64
    Name string
}

func (u *User) Orders(store order.Store) ([]order.Order, error) {
    return store.GetByUser(u.ID)
}
// No hay ciclo — user importa order, pero order NO importa user
```

---

## 6. internal/ — visibilidad restringida

### 6.1 El mecanismo internal

Go tiene **un unico mecanismo** para restringir visibilidad mas alla de exportado/no-exportado: el directorio `internal/`. Cualquier paquete dentro de `internal/` solo puede ser importado por paquetes que compartan el mismo **parent path**:

```
myproject/
├── go.mod (module myproject)
├── cmd/
│   └── server/
│       └── main.go       // Puede importar myproject/internal/*
├── internal/
│   ├── auth/
│   │   └── auth.go       // Solo importable desde myproject/**
│   ├── database/
│   │   └── db.go         // Solo importable desde myproject/**
│   └── middleware/
│       └── middleware.go  // Solo importable desde myproject/**
├── pkg/
│   └── models/
│       └── user.go       // Importable por cualquiera
└── api/
    └── handler.go        // Puede importar myproject/internal/*
```

```go
// myproject/cmd/server/main.go
package main

import (
    "myproject/internal/auth"      // OK — dentro de myproject/
    "myproject/internal/database"  // OK
    "myproject/pkg/models"         // OK — pkg/ es publico
)
```

```go
// otro_proyecto/main.go
package main

import (
    "myproject/pkg/models"         // OK — pkg/ es publico
    "myproject/internal/auth"      // ERROR: use of internal package not allowed
    "myproject/internal/database"  // ERROR
)
```

### 6.2 internal/ a distintos niveles

El mecanismo `internal/` funciona a cualquier nivel del arbol de directorios:

```
myproject/
├── go.mod
├── internal/              // Solo myproject/** puede importar
│   └── core/
├── pkg/
│   ├── api/
│   │   ├── internal/      // Solo myproject/pkg/api/** puede importar
│   │   │   └── validation/
│   │   ├── handler.go     // Puede importar pkg/api/internal/validation
│   │   └── router.go      // Puede importar pkg/api/internal/validation
│   └── models/
│       └── user.go        // NO puede importar pkg/api/internal/validation
└── cmd/
    └── server/
        └── main.go        // NO puede importar pkg/api/internal/validation
                           // PERO SI puede importar internal/core
```

```
REGLA DE internal/:

Un import path que contiene "internal/" solo es accesible
desde codigo que esta en el arbol cuya raiz es el directorio
PADRE de "internal/".

  parent/
  ├── internal/
  │   └── secret/     ← solo parent/** puede importar
  ├── sibling/        ← puede importar parent/internal/secret
  └── sub/
      └── deep/       ← puede importar parent/internal/secret

  external/            ← NO puede importar parent/internal/secret
```

### 6.3 Cuando usar internal/

```
USAR internal/ CUANDO:

1. Tienes un paquete que NECESITAN varios paquetes internos
   pero NO quieres que usuarios externos dependan de el
   Ejemplo: internal/database con helpers especificos del esquema

2. Quieres la libertad de cambiar la API sin romper compatibilidad
   Ejemplo: internal/parser puede cambiar su API en cada release
   porque nadie externo depende de ella

3. Implementaciones que no son parte de tu API publica
   Ejemplo: internal/cache con detalles de implementacion del cache

NO USAR internal/ CUANDO:

1. El paquete es genuinamente util para otros proyectos
   → Ponlo en pkg/ o en la raiz

2. Es un paquete tiny que solo usa un archivo
   → Dejalo como no exportado dentro del paquete que lo usa

3. Estas abusando para "esconder" mala organizacion
   → Reorganiza primero
```

---

## 7. La funcion init()

### 7.1 Que es init()

`init()` es una funcion especial que se ejecuta automaticamente cuando un paquete es importado. No recibe parametros, no retorna nada, y no se puede llamar manualmente:

```go
package database

import (
    "database/sql"
    "log"
    "os"
)

var db *sql.DB

func init() {
    var err error
    dsn := os.Getenv("DATABASE_URL")
    if dsn == "" {
        dsn = "postgres://localhost/mydb?sslmode=disable"
    }
    db, err = sql.Open("postgres", dsn)
    if err != nil {
        log.Fatalf("failed to open database: %v", err)
    }
    if err = db.Ping(); err != nil {
        log.Fatalf("failed to connect to database: %v", err)
    }
    log.Println("database connection established")
}

func GetDB() *sql.DB {
    return db
}
```

### 7.2 Reglas de init()

```
REGLAS DE init()

1. Se ejecuta AUTOMATICAMENTE al importar el package
   No hay forma de llamarla manualmente: init() no es un identificador valido.

2. Un archivo puede tener MULTIPLES init()
   Se ejecutan en el orden en que aparecen en el archivo.

3. Un package puede tener init() en MULTIPLES archivos
   Se ejecutan en orden alfabetico de archivos (a.go antes que b.go).

4. ORDEN DE EJECUCION:
   a. Primero se inicializan los packages importados (recursivamente)
   b. Luego las variables globales del package
   c. Luego las funciones init()
   d. Finalmente main() (si es package main)

5. Cada package se inicializa UNA SOLA VEZ
   Aunque lo importen multiples packages, init() corre una sola vez.

6. init() se ejecuta en un SOLO goroutine
   No hay concurrencia durante la inicializacion.
```

```go
// Orden de ejecucion — ejemplo

// archivo: a.go
package mypackage

var A = computeA() // 1. Variables globales (por orden de dependencia)

func init() {
    fmt.Println("init de a.go") // 2. Primer init (a.go es antes que b.go)
}

// archivo: b.go
package mypackage

var B = computeB()

func init() {
    fmt.Println("init de b.go — primero") // 3. Primer init de b.go
}

func init() {
    fmt.Println("init de b.go — segundo") // 4. Segundo init de b.go
}
```

### 7.3 Cuando usar y cuando evitar init()

```
USOS LEGITIMOS de init():

1. REGISTRAR drivers/plugins:
   func init() { sql.Register("postgres", &Driver{}) }
   func init() { prometheus.MustRegister(myMetric) }
   
2. VERIFICAR precondiciones del entorno:
   func init() {
       if os.Getenv("API_KEY") == "" {
           log.Fatal("API_KEY environment variable required")
       }
   }

3. COMPUTAR tablas/caches constantes:
   var crcTable *crc32.Table
   func init() { crcTable = crc32.MakeTable(crc32.IEEE) }

EVITAR init() CUANDO:

1. INICIALIZAR estado mutable complejo
   Problema: no puedes pasar parametros, no puedes retornar errores,
   dificulta testing. Mejor: usar un constructor explicito.
   
   // MAL
   var db *sql.DB
   func init() { db, _ = sql.Open(...) } // Error silenciado!
   
   // BIEN
   func NewDB(dsn string) (*sql.DB, error) {
       return sql.Open("postgres", dsn)
   }

2. EFECTOS SECUNDARIOS sorprendentes
   Si alguien importa tu package y algo inesperado ocurre
   (conexion a red, crear archivos, etc.), eso es un mal init().

3. TESTING
   init() se ejecuta SIEMPRE, incluso en tests.
   No hay forma de "apagar" un init().
   Esto hace que los tests sean fragiles y acoplados.

4. ORDEN IMPORTA
   Si el orden entre init() de distintos packages importa,
   tu diseno tiene un problema. init() no garantiza orden entre packages
   (solo que importados van antes que importadores).
```

### 7.4 Alternativa a init(): constructores explicitos

```go
// PATRON PREFERIDO: constructor en vez de init()

package config

type Config struct {
    DBHost     string
    DBPort     int
    LogLevel   string
    MaxWorkers int
}

// En vez de init() que lee env vars automaticamente:
// Usa un constructor explicito que se llama desde main()

func Load() (*Config, error) {
    host := os.Getenv("DB_HOST")
    if host == "" {
        return nil, fmt.Errorf("DB_HOST is required")
    }
    
    port, err := strconv.Atoi(os.Getenv("DB_PORT"))
    if err != nil {
        return nil, fmt.Errorf("invalid DB_PORT: %w", err)
    }
    
    return &Config{
        DBHost:     host,
        DBPort:     port,
        LogLevel:   getenvDefault("LOG_LEVEL", "info"),
        MaxWorkers: getenvIntDefault("MAX_WORKERS", 4),
    }, nil
}

func getenvDefault(key, defaultVal string) string {
    if v := os.Getenv(key); v != "" {
        return v
    }
    return defaultVal
}

func getenvIntDefault(key string, defaultVal int) int {
    if v := os.Getenv(key); v != "" {
        if n, err := strconv.Atoi(v); err == nil {
            return n
        }
    }
    return defaultVal
}
```

```go
// main.go — todo explicito, testable, sin magia
package main

import (
    "log"
    "myproject/config"
    "myproject/database"
    "myproject/server"
)

func main() {
    cfg, err := config.Load()
    if err != nil {
        log.Fatalf("config: %v", err)
    }
    
    db, err := database.Connect(cfg.DBHost, cfg.DBPort)
    if err != nil {
        log.Fatalf("database: %v", err)
    }
    defer db.Close()
    
    srv := server.New(cfg, db)
    log.Fatal(srv.ListenAndServe())
}
```

---

## 8. Estructura de un proyecto Go

### 8.1 Layout estandar (de facto)

No hay un layout "oficial" impuesto por el lenguaje, pero hay un consenso de facto en la comunidad:

```
myproject/
├── go.mod                    # Definicion del modulo
├── go.sum                    # Checksums de dependencias
├── main.go                   # Punto de entrada (si es un unico binario)
│
├── cmd/                      # Ejecutables
│   ├── server/
│   │   └── main.go           # go build ./cmd/server
│   ├── worker/
│   │   └── main.go           # go build ./cmd/worker
│   └── migrate/
│       └── main.go           # go build ./cmd/migrate
│
├── internal/                 # Paquetes privados al modulo
│   ├── auth/
│   │   ├── auth.go
│   │   ├── jwt.go
│   │   └── auth_test.go
│   ├── database/
│   │   ├── postgres.go
│   │   ├── migrations.go
│   │   └── database_test.go
│   ├── handler/
│   │   ├── user.go
│   │   ├── order.go
│   │   └── handler_test.go
│   └── middleware/
│       ├── logging.go
│       ├── recovery.go
│       └── middleware_test.go
│
├── pkg/                      # Paquetes reutilizables por otros proyectos
│   ├── models/               # (NO todos los proyectos necesitan pkg/)
│   │   └── user.go
│   └── httpclient/
│       └── client.go
│
├── api/                      # Definiciones de API (OpenAPI, protobuf, etc.)
│   └── openapi.yaml
│
├── configs/                  # Archivos de configuracion
│   └── config.yaml
│
├── scripts/                  # Scripts de build, deploy, etc.
│   └── migrate.sh
│
├── docs/                     # Documentacion
│   └── architecture.md
│
├── Makefile                  # Targets de build
├── Dockerfile                # Container image
└── README.md
```

### 8.2 Cuando NO necesitas este layout

```
PROYECTOS SIMPLES — no compliques

Para una CLI tool o un paquete library, este layout es overkill.

CLI tool simple:
  mytool/
  ├── go.mod
  ├── main.go
  ├── config.go
  ├── process.go
  └── process_test.go

Library package:
  mylib/
  ├── go.mod
  ├── mylib.go
  ├── mylib_test.go
  ├── helpers.go
  └── doc.go

Regla: empieza con la estructura mas simple posible.
Agrega cmd/, internal/, pkg/ solo cuando la complejidad lo justifique.
Un proyecto con 3 archivos no necesita 5 directorios.
```

### 8.3 Sobre pkg/: controversia

```
pkg/ — usarlo o no?

ARGUMENTO A FAVOR:
  - Comunica claramente "estos paquetes son API publica"
  - Distingue de internal/ (privado) vs pkg/ (publico)
  - Muchos proyectos grandes lo usan (Kubernetes, etcd, etc.)

ARGUMENTO EN CONTRA (opinion de muchos gophers):
  - Go ya tiene un mecanismo de visibilidad: internal/
  - TODO lo que NO esta en internal/ ya es publico
  - pkg/ agrega un nivel de directorio innecesario
  - Los imports se hacen mas largos:
    "myproject/pkg/models" vs "myproject/models"

RECOMENDACION:
  - Proyectos medianos/grandes con mucho internal/: usa pkg/ para claridad
  - Proyectos pequenos o libraries: no uses pkg/, pon todo en la raiz
  - NUNCA pongas todo en pkg/ — si todo es pkg/, nada lo es
```

---

## 9. Documentacion de paquetes

### 9.1 Comentarios de documentacion

Go genera documentacion automaticamente a partir de los comentarios que preceden las declaraciones:

```go
// Package auth provides authentication and authorization functionality
// for the myproject web application. It supports JWT tokens, API keys,
// and session-based authentication.
//
// Basic usage:
//
//	service := auth.NewService(config)
//	token, err := service.Authenticate(credentials)
//	if err != nil {
//	    log.Fatal(err)
//	}
package auth

// Service handles authentication operations.
// It is safe for concurrent use.
type Service struct {
    config Config
    store  TokenStore
}

// NewService creates a new authentication service with the given configuration.
// It returns an error if the configuration is invalid.
func NewService(cfg Config) (*Service, error) {
    if cfg.SecretKey == "" {
        return nil, errors.New("auth: secret key is required")
    }
    return &Service{config: cfg}, nil
}

// Authenticate validates credentials and returns a JWT token.
// It returns ErrInvalidCredentials if the credentials are wrong.
func (s *Service) Authenticate(creds Credentials) (string, error) {
    // ...
}
```

### 9.2 Reglas de documentacion

```
DOCUMENTACION EN GO

1. El comentario va INMEDIATAMENTE antes de la declaracion (sin linea en blanco)

2. Empieza con el NOMBRE del identificador:
   // NewService creates a new authentication service.
   func NewService(...) { ... }
   
   // Service handles authentication operations.
   type Service struct { ... }
   
   // ErrNotFound is returned when the requested resource does not exist.
   var ErrNotFound = errors.New("not found")

3. Parrafos separados por lineas en blanco en los comentarios

4. Codigo de ejemplo se indenta con un tab adicional:
   // Usage:
   //
   //	s := auth.NewService(cfg)
   //	token, _ := s.Authenticate(creds)

5. Package comment: describe el package, va en cualquier archivo
   (convencion: ponerlo en doc.go o en el archivo principal)
```

### 9.3 El archivo doc.go

Para documentacion extensa de un package, la convencion es crear un archivo `doc.go` que solo contiene el comentario del package:

```go
// Package sort provides primitives for sorting slices and user-defined
// collections.
//
// # Types of sorts
//
// This package implements several sorting algorithms, chosen based on
// the characteristics of the input...
//
// # Examples
//
//	// Sort a slice of ints
//	sort.Ints([]int{3, 1, 4, 1, 5})
//
//	// Sort with a custom comparator
//	sort.Slice(people, func(i, j int) bool {
//	    return people[i].Age < people[j].Age
//	})
package sort
```

### 9.4 go doc y pkgsite

```bash
# Ver documentacion en terminal
go doc fmt                    # Package-level doc
go doc fmt.Println            # Funcion especifica
go doc -all fmt               # Toda la documentacion del package
go doc -src fmt.Println       # Ver el codigo fuente

# Servidor de documentacion local
# (reemplaza al antiguo godoc)
go install golang.org/x/pkgsite/cmd/pkgsite@latest
pkgsite                       # Abre http://localhost:8080
                              # Muestra docs de todos tus modulos locales
```

---

## 10. Build constraints (build tags)

### 10.1 Compilacion condicional

Go permite incluir/excluir archivos de la compilacion basandose en el OS, arquitectura, u otros tags:

```go
// archivo: server_linux.go
// Solo se compila en Linux (por el nombre del archivo)

package server

import "syscall"

func setSocketOptions(fd int) error {
    return syscall.SetsockoptInt(fd, syscall.SOL_SOCKET, syscall.SO_REUSEPORT, 1)
}
```

```go
// archivo: server_darwin.go
// Solo se compila en macOS

package server

func setSocketOptions(fd int) error {
    // macOS tiene diferente API para socket options
    return nil
}
```

```go
// archivo: server_windows.go
// Solo se compila en Windows

package server

func setSocketOptions(fd int) error {
    // Windows no soporta SO_REUSEPORT
    return nil
}
```

### 10.2 Build constraints con //go:build

Desde Go 1.17, la sintaxis preferida usa `//go:build` con expresiones booleanas:

```go
//go:build linux && amd64

package mypackage
// Este archivo solo se compila en Linux AMD64
```

```go
//go:build !windows

package mypackage
// Este archivo se compila en todo EXCEPTO Windows
```

```go
//go:build (linux || darwin) && !cgo

package mypackage
// Solo Linux o macOS, y sin CGo habilitado
```

```go
//go:build ignore

package mypackage
// Este archivo NUNCA se compila (util para ejemplos, generadores, etc.)
```

### 10.3 Custom build tags

```go
//go:build integration

package database_test

// Este test solo corre si pasas -tags integration
func TestDatabaseConnection(t *testing.T) {
    db, err := Connect("postgres://localhost/testdb")
    if err != nil {
        t.Fatal(err)
    }
    defer db.Close()
    // ...
}
```

```bash
# Correr tests normales (excluye integration)
go test ./...

# Correr tests con tag integration
go test -tags integration ./...

# Build con tag custom
go build -tags "integration debug" ./cmd/server
```

### 10.4 Constraint por nombre de archivo

Go infiere build constraints del nombre del archivo automaticamente — sin necesidad de `//go:build`:

```
CONSTRAINTS POR NOMBRE DE ARCHIVO

*_GOOS.go           → compilar solo en ese OS
*_GOARCH.go         → compilar solo en esa arquitectura
*_GOOS_GOARCH.go    → compilar solo en esa combinacion

Ejemplos:
  file_linux.go         → solo Linux
  file_windows.go       → solo Windows
  file_darwin.go        → solo macOS
  file_amd64.go         → solo AMD64
  file_arm64.go         → solo ARM64
  file_linux_amd64.go   → solo Linux en AMD64

Tambien:
  *_test.go             → solo en testing (go test)
```

---

## 11. go:generate

`go:generate` es una directiva que ejecuta comandos para generar codigo automaticamente:

```go
//go:generate stringer -type=Color
//go:generate mockgen -source=store.go -destination=mock_store.go

package mypackage

type Color int

const (
    Red Color = iota
    Green
    Blue
)

// Despues de ejecutar "go generate ./...", se crea un archivo
// color_string.go con un metodo String() para Color:
// func (c Color) String() string { ... }
```

```bash
# Ejecutar todos los //go:generate en el proyecto
go generate ./...

# Ejecutar solo en un package
go generate ./internal/models/
```

```
go:generate — USOS COMUNES

1. STRINGER: generar String() para enums
   //go:generate stringer -type=Status

2. MOCKGEN: generar mocks para testing
   //go:generate mockgen -source=repo.go -destination=mock_repo.go

3. PROTOBUF: generar codigo desde .proto
   //go:generate protoc --go_out=. --go-grpc_out=. api.proto

4. EMBED: generar bindata (antes de go:embed)
   //go:generate go-bindata -pkg assets -o assets.go templates/

5. SQLC: generar Go types desde SQL
   //go:generate sqlc generate

NOTA: go generate NO se ejecuta automaticamente durante go build.
Debes ejecutarlo manualmente o en tu CI/CD pipeline.
Los archivos generados se commitean al repositorio normalmente.
```

---

## 12. go:embed — incrustar archivos en el binario

### 12.1 Basico

Desde Go 1.16, puedes incrustar archivos directamente en el binario compilado:

```go
package main

import (
    "embed"
    "fmt"
    "net/http"
)

// Incrustar un solo archivo como string
//go:embed version.txt
var version string

// Incrustar un solo archivo como bytes
//go:embed config.json
var defaultConfig []byte

// Incrustar multiples archivos/directorios como filesystem
//go:embed templates/*
var templateFS embed.FS

// Incrustar con patrones
//go:embed static/*.css static/*.js static/*.html
var staticFiles embed.FS

func main() {
    fmt.Println("Version:", version)
    
    // Usar embed.FS como filesystem
    data, err := templateFS.ReadFile("templates/index.html")
    if err != nil {
        panic(err)
    }
    fmt.Println(string(data))
    
    // Servir archivos embebidos via HTTP
    http.Handle("/static/", http.FileServer(http.FS(staticFiles)))
    http.ListenAndServe(":8080", nil)
}
```

### 12.2 Reglas de go:embed

```
REGLAS DE go:embed

1. La directiva va INMEDIATAMENTE antes de la variable (sin linea en blanco)

2. Tipos validos:
   //go:embed file.txt
   var s string          // Contenido como string (un solo archivo)
   
   //go:embed file.bin
   var b []byte           // Contenido como bytes (un solo archivo)
   
   //go:embed dir/*
   var fs embed.FS        // Filesystem virtual (uno o mas archivos/dirs)

3. Patrones glob:
   //go:embed *.go         // Todos los .go en el directorio actual
   //go:embed dir/*         // Todo en dir/ (excepto archivos que empiezan con . o _)
   //go:embed dir/**        // NO FUNCIONA — Go embed no soporta **
   //go:embed a.txt b.txt   // Multiples archivos en una linea
   
4. NO se incrustan:
   - Archivos que empiezan con . o _ (a menos que se nombren explicitamente)
   - Directorios que empiezan con . o _
   
5. La variable DEBE ser a nivel de paquete (no local a una funcion)

6. Los archivos deben estar en el mismo directorio del .go o subdirectorios
   No puedes hacer //go:embed ../other/file.txt
```

---

## 13. Comparacion con C y Rust

### 13.1 Sistema de modulos/paquetes

```
┌─────────────────────┬──────────────────────────┬──────────────────────────┬────────────────────────────┐
│ Concepto             │ Go                       │ C                        │ Rust                       │
├─────────────────────┼──────────────────────────┼──────────────────────────┼────────────────────────────┤
│ Unidad de codigo     │ Package (directorio)     │ Translation unit (.c)    │ Crate (proyecto completo)  │
│ Sub-unidad           │ — (flat dentro del pkg)  │ — (solo archivos)        │ mod (modulo, archivo o dir)│
│ Declaracion          │ package mypackage        │ N/A                      │ mod mymodule { }           │
│                      │                          │                          │ (o archivo mymodule.rs)    │
│ Import               │ import "path/to/pkg"     │ #include "header.h"      │ use crate::module::Item;   │
│ Publico              │ Mayuscula                │ extern + header           │ pub                        │
│ Privado              │ Minuscula                │ static                   │ (default)                  │
│ Semi-privado         │ internal/                │ No incluir en header     │ pub(crate), pub(super)     │
│ Import circular      │ PROHIBIDO                │ Posible (con cuidado)    │ Posible (mod tree)         │
│ Dependency manager   │ go mod (integrado)       │ No hay (CMake, Conan...) │ cargo (integrado)          │
│ Central registry     │ proxy.golang.org         │ No hay                   │ crates.io                  │
│ Compilacion          │ Package por package      │ Archivo por archivo      │ Crate completo             │
│ Build constraints    │ //go:build, _GOOS.go     │ #ifdef, #if              │ #[cfg(target_os = "...")]  │
│ Embed files          │ //go:embed               │ xxd, objcopy             │ include_str!, include_bytes│
│ Code generation      │ go:generate              │ Macros (limitadas)       │ proc_macro, build.rs       │
│ Init on import       │ init()                   │ __attribute__((construc))│ No (lazy_static, OnceLock) │
│ Doc generation       │ go doc, pkgsite          │ Doxygen (externo)        │ cargo doc (integrado)      │
└─────────────────────┴──────────────────────────┴──────────────────────────┴────────────────────────────┘
```

### 13.2 Comparacion detallada: modulos en Go vs Rust

```go
// Go: flat namespace dentro del package
// Todos los archivos en un directorio = un solo package
// No hay sub-modulos dentro de un package

// myproject/server/server.go
package server

type Server struct { ... }

// myproject/server/handler.go  
package server // MISMO package — ve todo de server.go

func (s *Server) handleRequest() { ... } // Puede usar Server directamente

// myproject/server/middleware.go
package server // MISMO package

func logging(next http.Handler) http.Handler { ... }
// Puede usar Server, handleRequest, logging — todo es el mismo scope
```

```rust
// Rust: modulos anidados con visibilidad granular
// Cada archivo puede ser un modulo, y puedes anidar modulos

// myproject/src/server/mod.rs (o server.rs)
pub mod handler;     // Declara sub-modulo "handler"
pub mod middleware;   // Declara sub-modulo "middleware"

pub struct Server { 
    port: u16,       // privado al modulo server
    pub addr: String, // publico
}

// myproject/src/server/handler.rs
use super::Server; // Importar del modulo padre

pub fn handle_request(s: &Server) {
    // Puede acceder a s.addr (pub)
    // NO puede acceder a s.port (privado al modulo server, no handler)
    // A menos que handler este DENTRO del modulo server
}

// myproject/src/server/middleware.rs
pub fn logging() { ... }
```

```
KEY DIFFERENCE:

Go:
  package = directory = flat namespace
  Todos los archivos comparten todo (exportado y no exportado)
  No hay privacidad DENTRO del package — solo hacia FUERA
  
Rust:
  module = archivo o directorio = hierarchical namespace
  Sub-modulos tienen su propia privacidad
  Puedes tener privacidad DENTRO del crate a distintos niveles
  pub(super), pub(crate), pub, (default private)
```

### 13.3 Comparacion: imports

```go
// Go imports — path-based, siempre desde la raiz del modulo
import (
    "fmt"                           // stdlib
    "net/http"                      // stdlib subpackage
    "github.com/gorilla/mux"        // externo (URL completa)
    "myproject/internal/auth"       // interno (ruta de modulo)
)

// Usar:
fmt.Println()
http.ListenAndServe()
mux.NewRouter()
auth.Verify()
```

```c
// C includes — textual inclusion, fragil
#include <stdio.h>          // System header
#include <stdlib.h>         // System header
#include "myproject/auth.h" // Local header (ruta relativa)
#include "utils.h"          // Local header

// Usar:
printf();                   // No hay namespace — todo global
my_auth_verify();           // Prefijo manual para evitar colisiones
```

```rust
// Rust use — tree-based, desde crate root
use std::fmt;                           // stdlib
use std::collections::HashMap;          // stdlib submodule
use tokio::sync::mpsc;                  // externo (declarado en Cargo.toml)
use crate::internal::auth;              // interno (desde crate root)
use super::sibling;                     // modulo hermano
use self::child;                        // sub-modulo

// Tree import (importar multiples items de un path):
use std::io::{self, Read, Write, BufReader};

// Usar:
fmt::format!();
HashMap::new();
mpsc::channel();
auth::verify();
```

---

## 14. Programa completo: CLI Tool con estructura de paquetes

Este programa demuestra un proyecto Go realista con multiples paquetes, visibilidad correcta, internal/, constructores explicitos (sin init()), y go:embed:

```
taskmanager/
├── go.mod
├── cmd/
│   └── taskmanager/
│       └── main.go
├── internal/
│   ├── config/
│   │   └── config.go
│   ├── storage/
│   │   └── storage.go
│   └── task/
│       └── task.go
├── pkg/
│   └── display/
│       └── display.go
└── templates/
    └── report.tmpl
```

### cmd/taskmanager/main.go

```go
package main

import (
    "fmt"
    "os"

    "taskmanager/internal/config"
    "taskmanager/internal/storage"
    "taskmanager/internal/task"
    "taskmanager/pkg/display"
)

func main() {
    cfg, err := config.Load()
    if err != nil {
        fmt.Fprintf(os.Stderr, "config error: %v\n", err)
        os.Exit(1)
    }

    store, err := storage.New(cfg.DataFile)
    if err != nil {
        fmt.Fprintf(os.Stderr, "storage error: %v\n", err)
        os.Exit(1)
    }

    if len(os.Args) < 2 {
        printUsage()
        os.Exit(1)
    }

    switch os.Args[1] {
    case "add":
        if len(os.Args) < 3 {
            fmt.Fprintln(os.Stderr, "usage: taskmanager add <title>")
            os.Exit(1)
        }
        t := task.New(os.Args[2])
        if err := store.Save(t); err != nil {
            fmt.Fprintf(os.Stderr, "save error: %v\n", err)
            os.Exit(1)
        }
        fmt.Printf("Task added: %s (ID: %s)\n", t.Title, t.ID)

    case "list":
        tasks, err := store.List()
        if err != nil {
            fmt.Fprintf(os.Stderr, "list error: %v\n", err)
            os.Exit(1)
        }
        display.Table(os.Stdout, tasks)

    case "done":
        if len(os.Args) < 3 {
            fmt.Fprintln(os.Stderr, "usage: taskmanager done <id>")
            os.Exit(1)
        }
        t, err := store.Get(os.Args[2])
        if err != nil {
            fmt.Fprintf(os.Stderr, "get error: %v\n", err)
            os.Exit(1)
        }
        t.MarkDone()
        if err := store.Save(t); err != nil {
            fmt.Fprintf(os.Stderr, "save error: %v\n", err)
            os.Exit(1)
        }
        fmt.Printf("Task completed: %s\n", t.Title)

    case "report":
        tasks, err := store.List()
        if err != nil {
            fmt.Fprintf(os.Stderr, "list error: %v\n", err)
            os.Exit(1)
        }
        if err := display.Report(os.Stdout, tasks); err != nil {
            fmt.Fprintf(os.Stderr, "report error: %v\n", err)
            os.Exit(1)
        }

    default:
        fmt.Fprintf(os.Stderr, "unknown command: %s\n", os.Args[1])
        printUsage()
        os.Exit(1)
    }
}

func printUsage() {
    fmt.Println("Usage: taskmanager <command> [args]")
    fmt.Println()
    fmt.Println("Commands:")
    fmt.Println("  add <title>   Add a new task")
    fmt.Println("  list          List all tasks")
    fmt.Println("  done <id>     Mark a task as done")
    fmt.Println("  report        Generate a summary report")
}
```

### internal/config/config.go

```go
package config

import (
    "os"
    "path/filepath"
)

// Config holds application configuration.
// Fields are exported for use by other internal packages.
type Config struct {
    DataFile string
    Verbose  bool
}

// Load creates a Config from environment variables with sensible defaults.
// No init() — explicitly called from main.
func Load() (*Config, error) {
    dataDir := os.Getenv("TASKMANAGER_DATA_DIR")
    if dataDir == "" {
        home, err := os.UserHomeDir()
        if err != nil {
            return nil, err
        }
        dataDir = filepath.Join(home, ".taskmanager")
    }

    if err := os.MkdirAll(dataDir, 0o755); err != nil {
        return nil, err
    }

    return &Config{
        DataFile: filepath.Join(dataDir, "tasks.json"),
        Verbose:  os.Getenv("TASKMANAGER_VERBOSE") == "1",
    }, nil
}
```

### internal/task/task.go

```go
package task

import (
    "crypto/rand"
    "encoding/hex"
    "time"
)

// Status represents the state of a task.
type Status int

const (
    Pending Status = iota
    Done
)

// statusNames maps Status values to display strings.
// Not exported — internal detail.
var statusNames = map[Status]string{
    Pending: "pending",
    Done:    "done",
}

func (s Status) String() string {
    if name, ok := statusNames[s]; ok {
        return name
    }
    return "unknown"
}

// Task represents a single task item.
type Task struct {
    ID        string    `json:"id"`
    Title     string    `json:"title"`
    Status    Status    `json:"status"`
    CreatedAt time.Time `json:"created_at"`
    DoneAt    time.Time `json:"done_at,omitempty"`
}

// New creates a new pending task with a random ID.
func New(title string) *Task {
    return &Task{
        ID:        generateID(),
        Title:     title,
        Status:    Pending,
        CreatedAt: time.Now(),
    }
}

// MarkDone transitions the task to Done status.
func (t *Task) MarkDone() {
    t.Status = Done
    t.DoneAt = time.Now()
}

// IsDone returns whether the task has been completed.
func (t *Task) IsDone() bool {
    return t.Status == Done
}

// generateID creates a short random hex ID.
// Not exported — internal helper.
func generateID() string {
    b := make([]byte, 4)
    rand.Read(b)
    return hex.EncodeToString(b)
}
```

### internal/storage/storage.go

```go
package storage

import (
    "encoding/json"
    "errors"
    "os"
    "sync"

    "taskmanager/internal/task"
)

// Store handles persistence of tasks to a JSON file.
type Store struct {
    path  string     // not exported — implementation detail
    mu    sync.Mutex // not exported — protects concurrent file access
}

// New creates a Store that reads/writes tasks to the given file path.
func New(path string) (*Store, error) {
    // Ensure the file exists
    if _, err := os.Stat(path); errors.Is(err, os.ErrNotExist) {
        if err := os.WriteFile(path, []byte("[]"), 0o644); err != nil {
            return nil, err
        }
    }
    return &Store{path: path}, nil
}

// List returns all stored tasks.
func (s *Store) List() ([]*task.Task, error) {
    s.mu.Lock()
    defer s.mu.Unlock()

    return s.readAll()
}

// Get retrieves a task by ID. Returns an error if not found.
func (s *Store) Get(id string) (*task.Task, error) {
    s.mu.Lock()
    defer s.mu.Unlock()

    tasks, err := s.readAll()
    if err != nil {
        return nil, err
    }

    for _, t := range tasks {
        if t.ID == id {
            return t, nil
        }
    }
    return nil, errors.New("task not found: " + id)
}

// Save creates or updates a task in the store.
func (s *Store) Save(t *task.Task) error {
    s.mu.Lock()
    defer s.mu.Unlock()

    tasks, err := s.readAll()
    if err != nil {
        return err
    }

    // Update if exists, append if new
    found := false
    for i, existing := range tasks {
        if existing.ID == t.ID {
            tasks[i] = t
            found = true
            break
        }
    }
    if !found {
        tasks = append(tasks, t)
    }

    return s.writeAll(tasks)
}

// readAll reads all tasks from the JSON file.
// Not exported — caller must hold s.mu.
func (s *Store) readAll() ([]*task.Task, error) {
    data, err := os.ReadFile(s.path)
    if err != nil {
        return nil, err
    }

    var tasks []*task.Task
    if err := json.Unmarshal(data, &tasks); err != nil {
        return nil, err
    }
    return tasks, nil
}

// writeAll writes all tasks to the JSON file atomically.
// Not exported — caller must hold s.mu.
func (s *Store) writeAll(tasks []*task.Task) error {
    data, err := json.MarshalIndent(tasks, "", "  ")
    if err != nil {
        return err
    }
    return os.WriteFile(s.path, data, 0o644)
}
```

### pkg/display/display.go

```go
// Package display provides output formatting for task data.
// This package is in pkg/ because it could be useful to external tools
// that want to display tasks in the same format.
package display

import (
    "embed"
    "fmt"
    "io"
    "text/tabwriter"
    "text/template"

    "taskmanager/internal/task"
)

//go:embed report.tmpl
var reportTemplate string

// Table writes tasks as a formatted table to w.
func Table(w io.Writer, tasks []*task.Task) {
    tw := tabwriter.NewWriter(w, 0, 4, 2, ' ', 0)
    fmt.Fprintln(tw, "ID\tTITLE\tSTATUS\tCREATED")
    fmt.Fprintln(tw, "──\t─────\t──────\t───────")
    for _, t := range tasks {
        fmt.Fprintf(tw, "%s\t%s\t%s\t%s\n",
            t.ID,
            t.Title,
            t.Status,
            t.CreatedAt.Format("2006-01-02 15:04"),
        )
    }
    tw.Flush()
}

// ReportData holds the computed data for a report.
type ReportData struct {
    Total    int
    Pending  int
    Done     int
    Tasks    []*task.Task
}

// Report generates a summary report using an embedded template.
func Report(w io.Writer, tasks []*task.Task) error {
    data := ReportData{
        Total: len(tasks),
        Tasks: tasks,
    }
    for _, t := range tasks {
        if t.IsDone() {
            data.Done++
        } else {
            data.Pending++
        }
    }

    tmpl, err := template.New("report").Parse(reportTemplate)
    if err != nil {
        return fmt.Errorf("parse template: %w", err)
    }
    return tmpl.Execute(w, data)
}
```

### templates/report.tmpl (embebido via go:embed)

```
=== Task Report ===

Total tasks:   {{.Total}}
Pending:       {{.Pending}}
Completed:     {{.Done}}

{{- if .Tasks}}

Tasks:
{{range .Tasks}}  [{{.Status}}] {{.Title}} ({{.ID}})
{{end}}
{{- else}}

No tasks found.
{{- end}}
```

```
PUNTOS CLAVE DEL PROGRAMA:

1. ESTRUCTURA DE PAQUETES
   cmd/          → ejecutable (package main)
   internal/     → paquetes privados (config, storage, task)
   pkg/          → paquete reutilizable (display)
   templates/    → archivos embebidos

2. VISIBILIDAD
   Task.ID        → exportado (otros paquetes lo leen)
   Store.path     → NO exportado (detalle de implementacion)
   Store.readAll  → NO exportado (helper interno)
   generateID()   → NO exportado (solo task lo usa)
   Status.String()→ exportado (satisface fmt.Stringer)

3. NO HAY init()
   Todo se inicializa explicitamente en main():
   config.Load() → storage.New() → operaciones

4. go:embed
   El template de reporte se incrusta en el binario.
   No necesita archivos externos en runtime.

5. NOMBRES SIN REPETICION
   task.Task       (no task.TaskItem)
   task.New         (no task.NewTask)
   storage.Store    (no storage.StorageStore)
   display.Table    (no display.DisplayTable)
   config.Load      (no config.LoadConfig)

6. INTERNAL CORRECTAMENTE USADO
   config, storage, task → detalles de implementacion
   Solo display esta en pkg/ (reutilizable externamente)
```

---

## 15. Ejercicios

### Ejercicio 1: Visibilidad por diseno
Crea un paquete `bank` con un tipo `Account` que tenga:
- Campos exportados: `Owner string`, `ID string`
- Campos no exportados: `balance int64`, `pin string`
- Constructor `New(owner, pin string) *Account` que genere un ID aleatorio
- Metodo exportado `Balance() int64` (getter)
- Metodo exportado `Deposit(amount int64) error` (con validacion)
- Metodo exportado `Withdraw(amount int64, pin string) error` (valida PIN y fondos)
- Metodo no exportado `validatePin(pin string) bool`
- Metodo no exportado `log(action string, amount int64)`

Desde un `package main`, intenta acceder a campos no exportados y verifica los errores del compilador.

### Ejercicio 2: internal/ en practica
Crea un proyecto con la estructura:
```
calculator/
├── cmd/calculator/main.go
├── internal/
│   ├── parser/parser.go    (parsea expresiones como "3 + 4 * 2")
│   └── evaluator/eval.go   (evalua el AST parseado)
└── pkg/
    └── calculator/calc.go  (API publica: Evaluate(expr string) (float64, error))
```
El paquete `pkg/calculator` usa `internal/parser` e `internal/evaluator`. Verifica que un proyecto externo puede importar `pkg/calculator` pero NO `internal/parser`.

### Ejercicio 3: Build constraints
Crea un paquete `sysinfo` que:
- En Linux: retorne informacion de `/proc/meminfo` y `/proc/cpuinfo`
- En macOS: retorne informacion de `sysctl`
- En Windows: retorne informacion de `runtime.GOOS` con un mensaje "limited info"
- Tenga una funcion comun `Summary() string` que funcione en todos los OS
Usa tanto constraints por nombre de archivo (`_linux.go`) como `//go:build`.

### Ejercicio 4: Refactoring de init()
Toma este codigo con init() problematico y refactoralo a constructores explicitos:
```go
package main

import (
    "database/sql"
    "log"
    "net/http"
    "os"
    _ "github.com/lib/pq"
)

var db *sql.DB
var logger *log.Logger

func init() {
    var err error
    db, err = sql.Open("postgres", os.Getenv("DATABASE_URL"))
    if err != nil { log.Fatal(err) }
    if err = db.Ping(); err != nil { log.Fatal(err) }
}

func init() {
    f, err := os.OpenFile("app.log", os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
    if err != nil { log.Fatal(err) }
    logger = log.New(f, "[APP] ", log.LstdFlags)
}

func main() {
    http.HandleFunc("/", handler)
    http.ListenAndServe(":8080", nil)
}

func handler(w http.ResponseWriter, r *http.Request) {
    logger.Printf("request: %s %s", r.Method, r.URL.Path)
    // usar db...
}
```
Refactoralo para que: (a) no use init(), (b) main() cree todo explicitamente, (c) los errores se manejen correctamente, (d) db y logger se cierren con defer.

---

> **Siguiente**: C09/S01 - Sistema de Paquetes, T02 - Biblioteca estandar destacada — os, io, fmt, strings, strconv, filepath, encoding/json, net/http, log/slog
